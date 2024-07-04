#pragma once
#include "Node.h"
#include "xxhash.h"

#include <filesystem>
#include <set>
#include <memory>

namespace YAM
{
    class ExecutionContext;
    class DirectoryNode;
    class Globber;

    // A GlobNode applies a glob path pattern to mirrored directories. 
    // The matching DirectoryNodes and/or SourceFileNodes are cached in the
    // GlobNode. Changes in the directories visited by the glob execution
    // will trigger re-execution of the glob node.
    //
    class __declspec(dllexport) GlobNode : public Node
    {
    public:
        GlobNode() {}; // needed for deserialization
        GlobNode(ExecutionContext* context, std::filesystem::path const& name);
        ~GlobNode();

        std::string className() const override { return "GlobNode"; }

        void baseDirectory(std::shared_ptr<DirectoryNode> const& newBaseDir);
        std::shared_ptr<DirectoryNode> const& baseDirectory() const { return _baseDir;  }

        // newPattern is relative to the base directory or is a symbolic
        // path (see FileRepositoryNode). newPattern can be a normal path
        // or a path that contains glob special characters. 
        void pattern(std::filesystem::path const& newPattern);
        std::filesystem::path const& pattern() const { return _pattern; }

        std::vector<std::shared_ptr<Node>> const& matches() const { return _matches; }

        void start(PriorityClass prio) override;

        // Optional: initialize the glob node by applying pattern() to the 
        // baseDirectory().
        // Pre: the glob node has never been executed and has not yet been
        // added to the context. Intended to be called immediately after 
        // construction.
        // Successfull initialize will move the glob to Node::State::Ok.
        void initialize();

        // Return a hash of the names of the nodes that match the glob pattern.
        XXH64_hash_t executionHash() const { return _executionHash; }

        static void setStreamableType(uint32_t type);
        // Inherited from IStreamable (via IPersistable)
        uint32_t typeId() const override;
        void stream(IStreamer* streamer) override;
        // Inherited from IPersistable
        void prepareDeserialize() override;
        bool restore(void* context, std::unordered_set<IPersistable const*>& restored) override;

    private:
        void destroy();
        void cleanup() override;
        std::pair<std::shared_ptr<Globber>, std::string> execute();
        void handleInputDirsCompletion(Node::State state);
        void executeGlob();
        void handleGlobCompletion(std::shared_ptr<Globber> const& globber, std::string const& error);

        XXH64_hash_t computeExecutionHash() const;
        XXH64_hash_t computeInputsHash() const;

        std::shared_ptr<DirectoryNode> _baseDir;
        std::filesystem::path _pattern;

        // _inputDirs are the directories that were queried by glob
        // execution.
        std::set<std::shared_ptr<DirectoryNode>, YAM::Node::CompareName> _inputDirs;

        // _inputsHash is a hash of the hashes of baseDir name, pattern and
        // input directories. A change in _inputsHash will trigger re-execution of 
        // the glob node.
        XXH64_hash_t _inputsHash;

        std::vector<std::shared_ptr<Node>> _matches; 
        // _executionHash is a hash of the names of matching nodes.
        // A change in _hash will trigger re-execution of nodes that depend
        // on this glob node.
        XXH64_hash_t _executionHash;
    };
}

