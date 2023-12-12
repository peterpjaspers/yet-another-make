#pragma once
#include "Node.h"

#include "../xxHash/xxhash.h"

#include <memory>
#include <vector>
#include <set>

namespace YAM {
    class FileNode;
    class SourceFileNode;
    class GeneratedFileNode;
    class CommandNode;
    class GlobNode;
    namespace BuildFile { class File; }

    // A BuildFileProcessingNode processes a buildfile to a collection of 
    // CommandNodes. The CommandNodes are added to the execution context.
    // 
    // Processing steps outline:
    //   - run the buildfile and parse its output into a BuildFile::File
    //   - extract buildfile and glob dependencies from the parse tree
    //   - process the buildfile dependencies
    //   - compile the BuildFile::File to CommandNodes
    //
    class __declspec(dllexport) BuildFileProcessingNode : public Node
    {
    public:
        BuildFileProcessingNode(); // needed for deserialization
        BuildFileProcessingNode(ExecutionContext* context, std::filesystem::path const& name);
        virtual ~BuildFileProcessingNode();

        void buildFile(std::shared_ptr<SourceFileNode> const& newFile);
        std::shared_ptr<SourceFileNode> buildFile() const;

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
        void setupBuildFileExecutor();
        void teardownBuildFileExecutor();
        void handleRequisitesCompletion(Node::State state);
        void parseBuildFile();
        void handleParseBuildFileCompletion(std::shared_ptr<BuildFile::File> tree, std::string error);
        void compile(std::shared_ptr<BuildFile::File> const& file);
        XXH64_hash_t computeExecutionHash() const;
        void notifyProcessingCompletion(Node::State state);


        // The buildfile to be processed by this processing node.
        std::shared_ptr<SourceFileNode> _buildFile;

        // _depFiles represent the input files of the buildfile.
        std::set<std::shared_ptr<FileNode>, Node::CompareName> _depFiles;

        // _depBFPN represent the buildfile dependencies declared in _buildFile.
        // These dependencies are extracted from the parsed buildfile. These
        // processing nodes must be executed before the parse result can be 
        // compiled in order to make sure that all input generated files  
        // referenced by _buildFile exist.
        std::set<std::shared_ptr<BuildFileProcessingNode>, Node::CompareName> _depBFPNs;

        // _depGlobs represent the glob dependencies declared in _buildFile.
        // These dependencies are extracted from the parsed buildfile. A change
        // in these globs will case this processing node to re-execute.
        std::set<std::shared_ptr<GlobNode>, Node::CompareName> _depGlobs;

        // The commands and generate file nodes compiled from the rules in the buildfile.
        std::set<std::shared_ptr<CommandNode>, Node::CompareName> _commands;
        std::set<std::shared_ptr<GeneratedFileNode>, Node::CompareName> _outputs;

        // _executionHash is the hash of the hashes of _depFiles, _depBFPNs 
        // and _depGlobs. A change in execution hash invalidates the compiled
        // CommandNodes and triggers re-processing of _buildFile.
        XXH64_hash_t _executionHash;

        // Running _buildfile is delegated to _buildFileExecutor.
        std::shared_ptr<CommandNode> _buildFileExecutor;

        // The output of running the _buildFile is redirected to this file.
        std::filesystem::path _tmpRulesFile;
    };
}

