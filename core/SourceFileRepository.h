#pragma once

#include "RegexSet.h"
#include "FileRepository.h"

#include <memory>
#include <filesystem>
#include <map>

namespace YAM
{
    class SourceDirectoryNode;
    class Node;
    class ExecutionContext;
    class FileRepositoryWatcher;

    // A SourceFileRepository mirrors a file repository in memory as a
    // SourceDirectoryNode that contains (sub-)DirectoryNodes and SourceFileNodes.
    // It stores these nodes in an ExecutionContext. It continuously watches
    // the repository for changes. See consumeChange()
    //
    // A SourceFileRepository cannot, and need not, mirror generated files.
    // Cannot because the GeneratedFileNode constructor takes a pointer to the
    // producer node which is not known by SourceFileRepository. 
    // Need not because GenerateFileNodes are created when build files are 
    // parsed, i.e. before the associated file even exists.
    // 
    // Because SourceFileRepository creates SourceFileNodes it must be able to
    // distinguish between source and generated files to avoid creating a
    // SourceFileInstance for a generated file. If this is not done properly
    // havoc will occur when that file is declared as an output file in a 
    // build file. In that case the build file parser will fail to create a
    // GeneratedFileNode for that file because SourceFileRepository already 
    // created a SourceFileNode with that name. 
    // SourceFileRepository relies entirely on exclude patterns to distinguish
    // source from generated files. A SourceFileNode will only be created for
    // a file whose path name does not matches one of the exclude patterns.
    // 
    // Note that exclude patterns can also be used to exclude source files
    // from being mirrored. This is usefull when a repository contains many
    // files that are not input files for the build. Excluding these files
    // will limit the size of the build graph.
    // 
    // The intended use of SourceFileRepository is to create source file
    // nodes before executing build file and command nodes.
    // Rationale: assume that the source file node for file F would be created 
    // (and its hashes computed) after it is detected as input of command node C.
    // In this scenario the following race condition may occur: the user edits F
    // in the time interval between completion of the command script of C and 
    // the subsequent retrieval of F's last-write-time and computation of F's 
    // aspect hashes. In this case the next build will not detect that F has 
    // changed (because F's last-write-time has not changed). As a result C will 
    // not re-execute, resulting in wrong content of C's output files.
    // 
    // This problem can be fixed in several ways:
    //    1- Do not allow modification of source files during the build.
    //    2- Capture last-write-time and hash code of F before using it.
    //       Because input files are detected during command node execution
    //       this can can only be implemented by creating file nodes for all 
    //       files in the worktree before starting command node execution.
    //    3- Detect which source files are modified during the build.
    //       At next build force the commands that used these files as input
    //       to re-execute.      
    // Option 1 is not user-friendly and not easy to implement.
    // Option 2 is easy to implement and simplifies handling input file
    // detection during command node execution because the file node and its 
    // hashes already exist at that time. Disadvantage is that it may result in
    // more file nodes than actually used as inputs.
    // Option 3 may cause unnecessary re-builds in case the file was modified 
    // before it was used as input. Also different commands may use the same 
    // input at different times. This complicates the implementation.
    //
    // Option 2 does not apply to generated files. Generated files are, like
    // source files, created and hashed before command execution. But after 
    // command execution their hashes have to be re-computed because the 
    // execution changes their content. This creates the possibility for the
    // earlier described race condition. Option 3 is hard to implement because
    // it is hard to distinguish between modifications resulting from command
    // execution and from the user tampering with the output files.
    //
    class __declspec(dllexport) SourceFileRepository : public FileRepository
    {
    public:
        // Recursively mirror source directories and source files in given 
        // 'directory' in a SourceDirectoryNode. Add the mirrored source directories
        // and source file nodes to given 'context->nodes()'.
        // Exclude from mirroring the directories and files whose paths match 
        // given '_excludePatterns'. Make sure that these patterns exclude all
        // generated files. 
        // The patterns may also exclude source directories and source files. 
        // A build will fail when it attempts to read from these source
        // directories and files.
        SourceFileRepository(
            std::string const& repoName,
            std::filesystem::path const& directory,
            // TODO: do not pass exclude patterns here.
            // Instead let each SourceDirectoryNode retrieve exclude patterns from 
            // .yamignore, or if absent, from .gitignore.
            RegexSet const& _excludePatterns,
            ExecutionContext* context);
        
        std::shared_ptr<SourceDirectoryNode> directoryNode() const;
        RegexSet const& excludePatterns() const;

        // Consume the changes that occurred in the filesystem since the previous
        // consumption by marking directory and file nodes associated with these 
        // changes as Dirty. 
        // The mirror can be synced with the file system by executing the dirty
        // nodes in the directoryNode() tree.
        void consumeChanges();

        // Return whether dir/file identified by 'path' has changed since 
        // previous consumeChanges().
        bool hasChanged(std::filesystem::path const& path);

        // Recursively remove the directory node from context->nodes().
        // Intended to be used when the repo is no longer to be mirrored
        // by YAM.
        void clear();

    private:
        RegexSet _excludePatterns;
        ExecutionContext* _context;
        std::shared_ptr<FileRepositoryWatcher> _watcher;
        std::shared_ptr<SourceDirectoryNode> _directoryNode;
    };
}
