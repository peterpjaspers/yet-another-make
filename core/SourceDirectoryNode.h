#pragma once
#include "Node.h"
#include "../xxHash/xxhash.h"

#include <chrono>
#include <vector>
#include <map>

namespace YAM
{
    class ExecutionContext;
    class FileNode;
    class DotIgnoreNode;

    // A SourceDirectoryNode keeps track of the source files in a directory:
    //    - creates a SourceFileNode for each file in the directory that is
    //      readAllowed by the FileRepository that contains that file.
    //      - creates a SourceDirectoryNode for each subdir in the directory that
    //      is readAllowed by the FileRepository that contains that subdir.
    //    - maintains the directory hash. This hash is computed from the
    //      names of the files and subdirs in the directory.
    // FileRepositories are registered in the execution context of the node.
    //
    class __declspec(dllexport) SourceDirectoryNode : public Node
    {
    public:
        SourceDirectoryNode() {} // needed for deserialization

        SourceDirectoryNode(ExecutionContext* context, std::filesystem::path const& dirName);

        // Inherited via Node
        virtual bool supportsPrerequisites() const override;
        virtual void getPrerequisites(std::vector<std::shared_ptr<Node>>& prerequisites) const override;

        virtual bool supportsPostrequisites() const override;
        virtual void getPostrequisites(std::vector<std::shared_ptr<Node>>& postrequisites) const override;

        virtual bool supportsOutputs() const override;
        virtual void getOutputs(std::vector<std::shared_ptr<Node>>& outputs) const override;

        virtual bool supportsInputs() const override;
        virtual void getInputs(std::vector<std::shared_ptr<Node>>& inputs) const override;
        // End of Inherited via Node

        // Query the directory content, vector content is sorted by node name.
        void getFiles(std::vector<std::shared_ptr<FileNode>>& filesInDir);
        void getSubDirs(std::vector<std::shared_ptr<SourceDirectoryNode>>& subDirsInDir);

        std::map<std::filesystem::path, std::shared_ptr<Node>> const& getContent(); // file + dir nodes

        std::chrono::time_point<std::chrono::utc_clock> const& lastWriteTime();

        // Pre: state() == State::Ok
        // Return the directory hash.
        XXH64_hash_t getHash() const;

        // Recursively remove the directory content from context->nodes().
        void clear();

        void execute();

        static void setStreamableType(uint32_t type);
        // Inherited from IStreamable
        uint32_t typeId() const override;
        void stream(IStreamer* streamer) override;
        // Inherited from IPersistable
        void restore(void* context) override;

    protected:
        // Inherited via Node
        virtual bool pendingStartSelf() const override;
        virtual void startSelf() override;

    private:
        std::shared_ptr<Node> createNode(std::filesystem::directory_entry const& dirEntry);
        bool updateLastWriteTime();
        void updateContent(
            std::filesystem::directory_entry const& dirEntry,
            std::map<std::filesystem::path, std::shared_ptr<Node>>& oldContent);
        void updateContent();
        void updateHash();
        void _removeChildRecursively(std::shared_ptr<Node> const& child);

        // last seen modification time of the directory
        std::chrono::time_point<std::chrono::utc_clock> _lastWriteTime;
        XXH64_hash_t _hash;

        std::shared_ptr<DotIgnoreNode> _dotIgnoreNode;
        std::map<std::filesystem::path, std::shared_ptr<Node>> _content;
    };
}


