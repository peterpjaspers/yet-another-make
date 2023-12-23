#pragma once
#include "Node.h"

#include "../xxHash/xxhash.h"

#include <memory>
#include <vector>
#include <unordered_set>
#include <map>

namespace YAM {
    class BuildFileParserNode;
    class SourceFileNode;
    class GeneratedFileNode;
    class CommandNode;
    class GlobNode;
    namespace BuildFile { class File; }

    // A BuildFileCompilerNode compiles the parse tree from a buildfile into a
    // set of CommandNodes.
    // 
    // Processing steps outline:
    //   - extract buildfile and glob dependencies from the parse tree
    //   - compile the buildfile dependencies
    //   - compile this buildfile to CommandNodes
    //
    class __declspec(dllexport) BuildFileCompilerNode : public Node
    {
    public:
        BuildFileCompilerNode(); // needed for deserialization
        BuildFileCompilerNode(ExecutionContext* context, std::filesystem::path const& name);
        virtual ~BuildFileCompilerNode();

        void buildFileParser(std::shared_ptr<BuildFileParserNode> const& newFile);
        std::shared_ptr<BuildFileParserNode> buildFileParser() const;

        // Inherited from Node
        void start() override;
        void getOutputs(std::vector<std::shared_ptr<Node>>& outputs) const override;
        void getInputs(std::vector<std::shared_ptr<Node>>& inputs) const override;

        static void setStreamableType(uint32_t type);
        // Inherited from IStreamer (via IPersistable)
        uint32_t typeId() const override;
        void stream(IStreamer* streamer) override;
        // Inherited from IPersistable
        void prepareDeserialize() override;
        void restore(void* context) override;

    private:
        bool updateBuildFileDependencies();
        void handleRequisitesCompletion(Node::State state);
        void compileBuildFile();
        void handleBuildFileDependenciesCompletion(Node::State state);
        XXH64_hash_t computeExecutionHash() const;
        bool validGeneratedInputs() const;
        BuildFileParserNode const* findDefiningParser(GeneratedFileNode const* genFile) const;
        void reportNotUsedParsers(std::unordered_set<BuildFileParserNode const*> const& usedParsers) const;
        void notifyProcessingCompletion(Node::State state);


        // The buildfile to be processed by this processing node.
        std::shared_ptr<BuildFileParserNode> _buildFileParser;

        // _depCompilers compile the buildfiles declared as dependencies of
        // this buildfile, extracted from _buildFileParser->parseTree().
        // These compiler nodes must be executed before this file can be 
        // compiled in order to make sure that all input generated files  
        // referenced by this buildfile exist.
        std::map<std::filesystem::path, std::shared_ptr<BuildFileCompilerNode>> _depCompilers;

        // _depGlobs represent the glob dependencies declared in the buildfile,
        // extracted from _buildFileParser->parseTree().
        std::map<std::filesystem::path, std::shared_ptr<GlobNode>> _depGlobs;

        // The commands and generated file nodes compiled from the rules in the buildfile.
        std::map<std::filesystem::path, std::shared_ptr<CommandNode>> _commands;
        std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> _outputs;

        // _executionHash is the hash of the hashes of _buildFileParser, 
        // _depBFPNs and _depGlobs. A change in execution hash invalidates the 
        // compiled CommandNodes and triggers re-processing of _buildFile.
        XXH64_hash_t _executionHash;
    };
}

