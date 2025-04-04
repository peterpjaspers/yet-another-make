#pragma once
#include "Node.h"

#include "xxhash.h"

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
    class GroupNode;
    class ForEachNode;
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

        std::string className() const override { return "BuildFileCompilerNode"; }

        void buildFileParser(std::shared_ptr<BuildFileParserNode> const& newFile);
        std::shared_ptr<BuildFileParserNode> buildFileParser() const;

        std::map<std::filesystem::path, std::shared_ptr<CommandNode>> const& commands() const {
            return _commands; 
        }
        std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> const & outputs() const {
            return _outputs;
        }
        std::map<std::filesystem::path, std::shared_ptr<GroupNode>> const& outputGroups() const {
            return _outputGroups;
        }
        
        // Inherited from Node
        void start(PriorityClass prio) override;
        void getOutputs(std::vector<std::shared_ptr<Node>>& outputs) const override;
        void getInputs(std::vector<std::shared_ptr<Node>>& inputs) const override;

        static void setStreamableType(uint32_t type);
        // Inherited from IStreamer (via IPersistable)
        uint32_t typeId() const override;
        void stream(IStreamer* streamer) override;
        // Inherited from IPersistable
        void prepareDeserialize() override;
        bool restore(void* context, std::unordered_set<IPersistable const*>& restored) override;

    private:
        bool updateBuildFileDependencies();
        void handleRequisitesCompletion(Node::State state);
        void compileBuildFile();
        void handleBuildFileDependenciesCompletion(Node::State state); 
        void cleanOutputGroup(GroupNode* group);
        XXH64_hash_t computeExecutionHash() const;
        bool validGeneratedInputs() const;
        bool findDefiningParsers(
            std::vector<std::shared_ptr<Node>> const& inputs,
            std::unordered_set<BuildFileParserNode const*>& parsers) const;
        BuildFileParserNode const* findDefiningParser(GeneratedFileNode const* genFile) const;
        void logNotUsedParserDependencies(std::unordered_set<BuildFileParserNode const*> const& usedParsers) const;
        void notifyProcessingCompletion(Node::State state);


        // _buildFileParser->parseTree is compiled by this compiler node.
        std::shared_ptr<BuildFileParserNode> _buildFileParser;

        // The compilers that compile the buildfiles declared as dependencies
        // in the buildfile parsed by _buildFileParser->buildFile.
        // These compilers must be executed before this file can be compiled in
        // order to make sure that all input generated files referenced by this 
        // buildfile exist.
        std::map<std::filesystem::path, std::shared_ptr<BuildFileCompilerNode>> _depCompilers;

        // _depGlobs represent the glob dependencies declared in the buildfile,
        // extracted from _buildFileParser->parseTree().
        std::map<std::filesystem::path, std::shared_ptr<GlobNode>> _depGlobs;

        // The commands and generated file nodes compiled from the rules in the buildfile.
        std::map<std::filesystem::path, std::shared_ptr<CommandNode>> _commands;
        std::map<std::filesystem::path, std::shared_ptr<ForEachNode>> _forEachNodes;
        std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> _outputs;
        std::map<std::filesystem::path, std::shared_ptr<GroupNode>> _outputGroups;

        // For each command/foreach node the line nr of the rule from which it was compiled.
        // Ordered as in _commands/_forEachNodes
        std::vector<std::size_t> _cmdRuleLineNrs;
        std::vector<std::size_t> _forEachRuleLineNrs;

        // _executionHash is the hash of the hashes of _buildFileParser, 
        // _depCompilers and _depGlobs. A change in execution requires re-compilation
        // of the buildfile parse tree.
        XXH64_hash_t _executionHash;
    };
}

