#pragma once
#include "FileNode.h"

#include <vector>
#include <memory>

namespace YAM
{
    class SourceDirectoryNode;
    class SourceFileNode;

    // A DotIgnoreNode parses a .gitignore and/or .yamignore file in a
    // given directory. Both files adhere to the gitignore specification,
    // see https://git-scm.com/docs/gitignore. The isIgnored() member function 
    // applies the precedence rules as specified in same specification.
    // 
    // YAM uses the patterns in these files to distinguish source files from
    // generated files (mandatory) and to exclude source files (optionally) 
    // that are not (allowed to be) used by the build from being mirrored by
    // SourceDirectoryNode.
    //
    class __declspec(dllexport) DotIgnoreNode : public Node
    {
    public:
        DotIgnoreNode() {} // needed for deserialization
        DotIgnoreNode(
            ExecutionContext* context,
            SourceDirectoryNode *directory);

        virtual ~DotIgnoreNode();

        virtual void setState(State newState) override;

        virtual bool supportsPrerequisites() const override;
        virtual void getPrerequisites(std::vector<std::shared_ptr<Node>>& prerequisites) const override;

        virtual bool supportsInputs() const override;
        virtual void getInputs(std::vector<std::shared_ptr<Node>>& inputs) const override;

        virtual bool pendingStartSelf() const override;

        virtual void startSelf() override;

        // return the hash of the ignore patterns. 
        XXH64_hash_t hash() const;

        // Return whether given path is not a source file or a source file that is
        // not allowed to be accessed by the build.
        bool ignore(std::filesystem::path const& path) const;

        // Remove the .gitignore and .yamignore nodes from context->nodes().
        void clear();

        static void setStreamableType(uint32_t type);
        // Inherited from IStreamable
        uint32_t typeId() const override;
        void stream(IStreamer* streamer) override;

    private:
        void setDotIgnoreFiles(std::vector<std::shared_ptr<SourceFileNode>> const& newFiles);
        XXH64_hash_t computeHash() const;
        void execute();

        SourceDirectoryNode* _directory;

        // The input files, i.e. the .gitignore and/or .yamignore files
        std::vector<std::shared_ptr<SourceFileNode>> _dotIgnoreFiles;
        // TODO: the patterns retrieved from the input files.
         
        // The hash of the hashes of the _dotIgnoreFiles.
        XXH64_hash_t _hash;
    };
}

