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
    class FileRepository;
    class BuildFileProcessingNode;

    // Executing a DirectoryNode caches the content of a directory as
    //    - a Source or Generated FileNode for each file in the directory.
    //    - a DirectoryNode for each subdir in the directory.
    // 
    // When executing a DirectoryNode it will:
    //      - synchronize its content with the filesystem state
    //      - execute all dirty sub DirectoryNodes
    // The DirectoryNode will NOT execute its dirty file nodes.
    // Rationale: executing file nodes (i.e. hashing file context) is expensive
    // and therefore only done on demand during the execution of nodes that 
    // depend on these file nodes.
    // 
    // DirectoryNode maintains the directory hash: a hash of the names
    // of the files and subdirs in the directory.
    // 
    // The first component of a directory/file node path is the name of the
    // repository that contains the directory/file.
    // 
    // All functions execute in main thread unless stated otherwise.
    //
    class __declspec(dllexport) DirectoryNode : 
        public Node,
        public std::enable_shared_from_this<DirectoryNode>
    {
    public:
        DirectoryNode() {} // needed for deserialization

        DirectoryNode(
            ExecutionContext* context, 
            std::filesystem::path const& dirName,
            DirectoryNode* parent);

        virtual ~DirectoryNode();

        void start() override;

        // Add the prerequisites (i.e. the DotIgnoreNode and its prerequisites)
        // to the execution context.
        void addPrerequisitesToContext();

        std::shared_ptr<DirectoryNode> parent() const;
        std::shared_ptr<DotIgnoreNode> const& dotIgnoreNode() { return _dotIgnoreNode; }

        // Query the directory content, vector content is sorted by node name.
        void getFiles(std::vector<std::shared_ptr<FileNode>>& filesInDir);
        void getSubDirs(std::vector<std::shared_ptr<DirectoryNode>>& subDirsInDir);

        void getOutputs(std::vector<std::shared_ptr<Node>>& outputs) const override;
        void getInputs(std::vector<std::shared_ptr<Node>>& inputs) const override;

        std::map<std::filesystem::path, std::shared_ptr<Node>> const& getContent() {
            return _content;
        }

        // Find and return the node identified by 'path'.
        // Pre: 'path' is relative to name(). 
        std::shared_ptr<Node> findChild(std::filesystem::path const& path);

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
        std::shared_ptr<Node> findChild(
            std::shared_ptr<DirectoryNode> directory,
            std::filesystem::path::iterator it,
            std::filesystem::path::iterator itEnd);

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
        void updateBuildFileProcessingNode();
        void _removeChildRecursively(std::shared_ptr<Node> const& child);

        // Next 3 functions execute in a threadpool thread
        std::chrono::time_point<std::chrono::utc_clock> retrieveLastWriteTime() const;
        std::shared_ptr<Node> getNode(
            std::filesystem::directory_entry const& dirEntry,
            std::shared_ptr<FileRepository> const& repo,
            std::unordered_set<std::shared_ptr<Node>>& added,
            std::unordered_set<std::shared_ptr<Node>>& kept);
        void retrieveContent(
            std::map<std::filesystem::path, std::shared_ptr<Node>>& content,
            std::unordered_set<std::shared_ptr<Node>>& added,
            std::unordered_set<std::shared_ptr<Node>>& kept,
            std::unordered_set<std::shared_ptr<Node>>& removed);

        DirectoryNode* _parent;
        std::shared_ptr<DotIgnoreNode> _dotIgnoreNode;
        std::shared_ptr<BuildFileProcessingNode> _buildFileProcessingNode;
        std::chrono::time_point<std::chrono::utc_clock> _lastWriteTime;
        std::map<std::filesystem::path, std::shared_ptr<Node>> _content;
        XXH64_hash_t _executionHash;
    };
}


