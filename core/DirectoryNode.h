#pragma once
#include "Node.h"
#include "MemoryLogBook.h"
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

    // A DirectoryNode caches the content of a directory:
    //    - creates a SourceFileNode for each file in the directory.
    //    - creates a DirectoryNode for each subdir in the directory.
    //    - maintains the directory hash. This hash is computed from the
    //      names of the files and subdirs in the directory.
    //
    // When executing a DirectoryNode it will:
    //      - synchronize its content with the filesystem state
    //      - execute all dirty sub DirectoryNodes
    // The DirectoryNode will  NOT execute its file nodes.
    // Rationale: executing (rehashing) file nodes is expensive and therefore
    // only done on demand during the execution of nodes that depend on these
    // file nodes.
    // 
    // All functions execute in main thread unless stated otherwise.
    //
    class __declspec(dllexport) DirectoryNode : public Node
    {
    public:
        DirectoryNode() {} // needed for deserialization

        DirectoryNode(
            ExecutionContext* context, 
            std::filesystem::path const& dirName,
            DirectoryNode* parent);

        void start() override;

        // Add the prerequisites (i.e. the DotIgnoreNode and its prerequisites)
        // to the execution context.
        void addPrerequisitesToContext();

        DirectoryNode* parent() const;
        std::shared_ptr<DotIgnoreNode> const& dotIgnoreNode() { return _dotIgnoreNode; }

        // Query the directory content, vector content is sorted by node name.
        void getFiles(std::vector<std::shared_ptr<FileNode>>& filesInDir);
        void getSubDirs(std::vector<std::shared_ptr<DirectoryNode>>& subDirsInDir);

        void getOutputs(std::vector<std::shared_ptr<Node>>& outputs) const override;
        void getInputs(std::vector<std::shared_ptr<Node>>& inputs) const override;

        std::map<std::filesystem::path, std::shared_ptr<Node>> const& getContent() {
            return _content;
        }

        std::chrono::time_point<std::chrono::utc_clock> const& lastWriteTime();

        // Pre: state() == State::Ok
        // Return the execution hash (hash of hash of all dir entry names)
        XXH64_hash_t executionHash() const;

        XXH64_hash_t computeExecutionHash(
            XXH64_hash_t dotIgnoreNodeHash,
            std::map<std::filesystem::path, std::shared_ptr<Node>> const& content) const;

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
        void handleDirtyOf(Node* observedNode) override;

    private:
        void parent(DirectoryNode* parent);

        struct RetrieveResult {
            Node::State _newState;
            MemoryLogBook _log;
            std::chrono::time_point<std::chrono::utc_clock> _lastWriteTime;
            std::map<std::filesystem::path, std::shared_ptr<Node>> _content;
            // the difference with _content and this->_content
            std::unordered_set<std::shared_ptr<Node>> _added;
            std::unordered_set<std::shared_ptr<Node>> _kept;
            std::unordered_set<std::shared_ptr<Node>> _removed;
            XXH64_hash_t _executionHash;
        };

        void handleRequisitesCompletion(Node::State state);
        void retrieveContentIfNeeded(); // Executes in a threadpool thread
        void handleRetrieveContentCompletion(RetrieveResult& result);
        void startSubDirs();
        void commitResult(YAM::DirectoryNode::RetrieveResult& result);
        void _removeChildRecursively(std::shared_ptr<Node> const& child);

        // Next 3 functions execute in a threadpool thread
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

        DirectoryNode* _parent;
        std::shared_ptr<DotIgnoreNode> _dotIgnoreNode;
        std::chrono::time_point<std::chrono::utc_clock> _lastWriteTime;
        std::map<std::filesystem::path, std::shared_ptr<Node>> _content;
        XXH64_hash_t _executionHash;
    };
}


