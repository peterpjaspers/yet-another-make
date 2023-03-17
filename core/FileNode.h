#pragma once

#include "Node.h"
#include "FileAspect.h"
#include "../xxHash/xxhash.h"

#include <map>

namespace YAM
{
    class FileRepository;

    // A file node computes hashes of aspects of its associated file. The list
    // of aspects applicable to the file is retrieved from the node's execution 
    // context.
    // 
    // FileNode::rehashAll() retrieves and caches the file's last-write-time
    // and then computes the aspect hashes of the file. rehashAll() is called
    // by the file node itself during its execution. If the file node is an
    // output node of some command node C then rehashAll() is also called
    // by C on completion of C's execution. 
    // 
    // Hashing a non-existing file results in a random hash value. An empty set
    // of aspects will only update the cached file's last-write-time.
    // 
    // The cached hash value of an aspect can be retrieved via the node's 
    // hashOf() function. An exception is thrown when retrieving the hash
    // of an aspect that is not known by the file node.
    // 
    // The intended use of file node aspect hashes is described for the
    // following scenarios (C and P are command nodes):
    //   1) Source file F is detected as input of C
    //   2) Output file F is produced by C
    //   3) Output file F, produced by command P, is detected as input of C
    //  
    // 1) Source file F is detected as input of C
    // C will act as follows:
    //     if source file node associated with F does not exist
    //         error //see SourceFileRepository
    //     else
    //         add F to C's input files and prerequisites
    //         use F.hashOf(aspect) to compute C's executionHash where
    //         aspect is the one applicable to C (e.g. when C is a C++ 
    //         compilation command then C will use the code aspect hash of F.
    //         The code aspect hash excludes comments from being hashed).
    // During next build: once C's prerequisites have been executed C's 
    // pendingStartSelf will compare the cached executionHash with the current
    // executionHash to detect whether re-execution of C is needed.
    // 
    // 2) Output file F is produced by C
    // C will act as follows:
    //     if output file node associated with F does not exist
    //         error //YAM requires all output nodes to be known a-priori
    //     else
    //         call F.rehashAll() 
    //         use F.hashOf(entireFile) to compute C's executionHash (the
    //         entireFile aspect hashes all content of F).
    // 
    // During next build: after C's prerequisites (which includes all of its
    // input and output nodes) have been executed C's pendingStartSelf will use
    // F.hashOf(entireFile) to compute C's executionHash and compare it with
    // C's previous executionHash to detect whether re-execution of C is needed.
    // 
    // 3) Output file F, produced by P is detected, as input of C
    // C will act as follows:
    //     add F to C's input files and prerequisites
    //     use F.hashOf(aspect) to compute C's executionHash where aspect is
    //     the one applicable to C (e.g. when C is a link command and F is a 
    //     dll import library then C will use the exports aspect hash of F. 
    //     The exports hash only hashes the exported symbols of F).
    //  
    // Race condition: a user may tamper with an output file in the time 
    // interval between its last update by the command script and the time of
    // the retrieval of its last-write-time. In this case the next build will
    // not detect that the file has changed (because last-write-time has not 
    // changed since its last retrieval) and will not re-execute the command, 
    // resulting in wrong content of the output file. 
    // This problem can be fixed by detecting, during the build, which output 
    // files are modified by other actors than commands and, at the next build,
    // force the commands that produced these files to re-execute.
    // 
    class __declspec(dllexport) FileNode : public Node
    {
    public:
        FileNode() {} // needed for deserialization

        // name is the absolute path name of the file
        FileNode(ExecutionContext* context, std::filesystem::path const& name);

        // Inherited via Node
        virtual bool supportsPrerequisites() const override;
        virtual void getPrerequisites(std::vector<std::shared_ptr<Node>>& prerequisites) const override;

        virtual bool supportsOutputs() const override;
        virtual void getOutputs(std::vector<std::shared_ptr<Node>>& outputs) const override;

        virtual bool supportsInputs() const override;
        virtual void getInputs(std::vector<std::shared_ptr<Node>>& inputs) const override;

        // Pre: state() == State::Ok
        // Return the cached hash of given aspect.
        // Throw exception when aspect is unknown.
        XXH64_hash_t hashOf(std::string const& aspectName);

        // Pre: state() == State::Ok
        // Retrieve and cache file's last-write-time, then compute and cache 
        // hashes of all applicable file aspects.
        void rehashAll();

        // Return the file repository that contains this file.
        std::shared_ptr<FileRepository> fileRepository() const;

        // Return the file path (this->name()) relative to the root directory
        // of the file repository that contains this file.
        std::filesystem::path relativePath() const;

        // Return fileRepository->symbolicPathOf(name()).
        std::filesystem::path symbolicPath() const;

        std::chrono::time_point<std::chrono::utc_clock> const& lastWriteTime();

        static void setStreamableType(uint32_t type);
        // Inherited from IStreamable
        uint32_t typeId() const override;
        void stream(IStreamer* streamer) override;

    protected:
        // Inherited via Node
        virtual bool pendingStartSelf() const override;
        virtual void startSelf() override;

    private:
        bool updateLastWriteTime();
        void rehashAll(bool doUpdateLastWriteTime);
        void execute();

        // last seen file modification time
        std::chrono::utc_clock::time_point _lastWriteTime;

        // aspect name => hash value. All hashes are computed after last
        // update of _lastWriteTime.
        std::map<std::string, XXH64_hash_t> _hashes;
    };
}

