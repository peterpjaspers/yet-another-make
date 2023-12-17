#pragma once
#include "CommandNode.h"
#include "BuildFile.h"

#include "../xxHash/xxhash.h"

#include <memory>
#include <vector>
#include <map>

namespace YAM {
    class SourceFileNode;
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

        static void setStreamableType(uint32_t type);
        // Inherited from IStreamer (via IPersistable)
        uint32_t typeId() const override;
        void stream(IStreamer* streamer) override;
        // Inherited from IPersistable
        void prepareDeserialize() override;
        void restore(void* context) override;

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
        std::shared_ptr<SourceFileNode> _buildFile;
        BuildFile::File _parseTree;
        XXH64_hash_t _parseTreeHash;
    };
}

