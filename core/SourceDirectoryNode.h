#pragma once
#include "Node.h"
#include "../xxHash/xxhash.h"

#include <chrono>
#include <vector>
#include <map>
#include <unordered_set>

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

        SourceDirectoryNode(
            ExecutionContext* context, 
            std::filesystem::path const& dirName,
            SourceDirectoryNode* parent);

        // Add the prerequisites (i.e. the DotIgnoreNode and its prerequisites)
        // to the execution context.
        void addPrerequisitesToContext();

        // Inherited via Node
        bool supportsPrerequisites() const override;
        void getPrerequisites(std::vector<std::shared_ptr<Node>>& prerequisites) const override;

        bool supportsPostrequisites() const override;
        void getPostrequisites(std::vector<std::shared_ptr<Node>>& postrequisites) const override;

        bool supportsOutputs() const override;
        void getOutputs(std::vector<std::shared_ptr<Node>>& outputs) const override;

        bool supportsInputs() const override;
        void getInputs(std::vector<std::shared_ptr<Node>>& inputs) const override;
        // End of Inherited via Node

        SourceDirectoryNode* parent() const;

        // Query the directory content, vector content is sorted by node name.
        void getFiles(std::vector<std::shared_ptr<FileNode>>& filesInDir);
        void getSubDirs(std::vector<std::shared_ptr<SourceDirectoryNode>>& subDirsInDir);

        std::map<std::filesystem::path, std::shared_ptr<Node>> const& getContent(); // file + dir nodes

        std::chrono::time_point<std::chrono::utc_clock> const& lastWriteTime();

        // Pre: state() == State::Ok
        // Return the execution hash (hash of hash of all dir entry names)
        XXH64_hash_t executionHash() const;

        // Recursively remove the directory content from context->nodes().
        void clear();

        static void setStreamableType(uint32_t type);
        // Inherited from IStreamable
        uint32_t typeId() const override;
        void stream(IStreamer* streamer) override;
        // Inherited from IPersistable
        void prepareDeserialize() override;
        void restore(void* context) override;

    protected:
        // Inherited via Node
        bool pendingStartSelf() const override;
        void selfExecute() override;
        void commitSelfCompletion(SelfExecutionResult const* result) override;

    private:
        void parent(SourceDirectoryNode* parent);

        struct ExecutionResult : public Node::SelfExecutionResult {
            std::chrono::time_point<std::chrono::utc_clock> _lastWriteTime;
            std::map<std::filesystem::path, std::shared_ptr<Node>> _content;
            // the difference with _content and this->_content
            std::unordered_set<std::shared_ptr<Node>> _added;
            std::unordered_set<std::shared_ptr<Node>> _kept;
            std::unordered_set<std::shared_ptr<Node>> _removed;
            XXH64_hash_t _executionHash;
        };

        std::chrono::time_point<std::chrono::utc_clock> retrieveLastWriteTime() const;
        std::shared_ptr<Node> getNode(
            std::filesystem::directory_entry const& dirEntry,
            std::unordered_set<std::shared_ptr<Node>>& added,
            std::unordered_set<std::shared_ptr<Node>>& kept) const;
        void retrieveContent(
            std::map<std::filesystem::path, std::shared_ptr<Node>>& content,
            std::unordered_set<std::shared_ptr<Node>>& added,
            std::unordered_set<std::shared_ptr<Node>>& kept,
            std::unordered_set<std::shared_ptr<Node>>& removed) const;
        XXH64_hash_t computeExecutionHash(std::map<std::filesystem::path, std::shared_ptr<Node>> const& content) const;
        void _removeChildRecursively(std::shared_ptr<Node> const& child);
        void selfExecute(ExecutionResult* result) const;

        SourceDirectoryNode* _parent;
        std::shared_ptr<DotIgnoreNode> _dotIgnoreNode;
        std::chrono::time_point<std::chrono::utc_clock> _lastWriteTime;
        std::map<std::filesystem::path, std::shared_ptr<Node>> _content;
        XXH64_hash_t _executionHash;
    };
}

