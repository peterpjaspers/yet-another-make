#pragma once
#include "CommandNode.h"
#include "BuildFile.h"

#include "../xxHash/xxhash.h"

#include <memory>
#include <vector>

namespace YAM {
    class SourceFileNode;
    class DirectoryNode;
    template<typename T> class AcyclicTrail;
    namespace BuildFile { class File; }

    // Excuting a BuildFileParserNode:
    //    - If the buildfile is a text file: parse the text file.
    //    - If the buildfile is an executable file: run the build file and
    //      parse its output.
    // 
    // See BuildFileParser for details on syntax.
    class __declspec(dllexport) BuildFileParserNode : public CommandNode
    {
    public:
        BuildFileParserNode(); // needed for deserialization
        BuildFileParserNode(ExecutionContext* context, std::filesystem::path const& name);

        void buildFile(std::shared_ptr<SourceFileNode> const& newFile);
        std::shared_ptr<SourceFileNode> buildFile() const;

        BuildFile::File const& parseTree() const; 
        XXH64_hash_t parseTreeHash() const;

        std::vector<BuildFileParserNode const*> const& dependencies();

        // Walk the dependency graph. Return whether no cycle was detected in
        // the dependency graph.
        bool walkDependencies(AcyclicTrail<BuildFileParserNode const*>& trail) const;

        static void setStreamableType(uint32_t type);
        // Inherited from IStreamer (via IPersistable)
        uint32_t typeId() const override;
        void stream(IStreamer* streamer) override;
        // Inherited from IPersistable
        void prepareDeserialize() override;
        bool restore(void* context, std::unordered_set<IPersistable const*>& restored) override;

    protected:
        struct ParseResult : public CommandNode::PostProcessResult {
            std::filesystem::path rulesFile;
            std::string parseErrors;
            std::shared_ptr<BuildFile::File> parseTree;
            XXH64_hash_t parseTreeHash;
        };
        // override CommandNode
        std::shared_ptr<CommandNode::PostProcessResult> postProcess(std::string const& stdOut) override;
        void commitPostProcessResult(std::shared_ptr<CommandNode::PostProcessResult>& result) override;

    private:
        bool composeDependencies();
        BuildFileParserNode* findDependency(std::filesystem::path const& depPath);

        std::shared_ptr<SourceFileNode> _buildFile;
        BuildFile::File _parseTree;
        XXH64_hash_t _parseTreeHash;
        // The (non-recursive_ dependencies on other buildfiles as declared in 
        // _parseTree->deps, expressed by their parser nodes.
        std::vector<BuildFileParserNode const*> _dependencies;
    };
}
