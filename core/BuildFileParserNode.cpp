#include "BuildFileParserNode.h"
#include "SourceFileNode.h"
#include "ExecutionContext.h"
#include "FileSystem.h"
#include "FileAspect.h"
#include "BuildFileParser.h"

namespace
{
    using namespace YAM;

    uint32_t streamableTypeId = 0;
}

namespace YAM
{

    BuildFileParserNode::BuildFileParserNode() {}

    BuildFileParserNode::BuildFileParserNode(ExecutionContext* context, std::filesystem::path const& name)
        : CommandNode(context, name)
    {}

    void BuildFileParserNode::buildFile(std::shared_ptr<SourceFileNode> const& newFile) {
        if (_buildFile != newFile) {
            if (_buildFile != nullptr) {
                CommandNode::script("");
            }
            _buildFile = newFile;
            if (_buildFile != nullptr) {
                if (_buildFile->name().extension() != ".txt") {
                // Execution is done with working directory being the build file
                // directory. So use filename().
                // TODO: find interpreter needed to run the stript
                // For now: only .bat files are supported.
                    std::string script = _buildFile->name().filename().string();
                    CommandNode::script(script);
                }
            }
            _buildFile->setState(Node::State::Dirty);
        }
    }

    std::shared_ptr<SourceFileNode> BuildFileParserNode::buildFile() const {
        return _buildFile;
    }

    BuildFile::File const& BuildFileParserNode::parseTree() const {
        if (state() != Node::State::Ok) throw std::runtime_error("illegal state");
        return _parseTree;
    }

    XXH64_hash_t BuildFileParserNode::parseTreeHash() const {
        if (state() != Node::State::Ok) throw std::runtime_error("illegal state");
        return _parseTreeHash;
    }

    std::shared_ptr<CommandNode::PostProcessResult> BuildFileParserNode::postProcess(std::string const& stdOut) {
        auto result = std::make_shared<ParseResult>();
        result->newState = Node::State::Ok;
        try {
            std::filesystem::path absBuildFilePath = _buildFile->absolutePath();
            if (_buildFile->name().extension() == ".txt") {
                BuildFileParser parser(absBuildFilePath);
                result->parseTree = parser.file();
            } else {
                BuildFileParser parser(stdOut, absBuildFilePath);
                result->parseTree = parser.file();
            }
            result->parseTreeHash = result->parseTree->computeHash();
            result->readOnlyFiles.insert(absBuildFilePath);
        } catch (std::runtime_error ex) {
            result->parseErrors = ex.what();
            result->newState = Node::State::Failed;
        }
        return result;
    }

    void BuildFileParserNode::commitPostProcessResult(std::shared_ptr<CommandNode::PostProcessResult>& sresult) {
        auto result = dynamic_pointer_cast<ParseResult>(sresult);
        if (result->newState != Node::State::Ok) {
            if (!result->parseErrors.empty()) {
                LogRecord error(LogRecord::Error, result->parseErrors);
                context()->logBook()->add(error);
            }
        } else {
            _parseTree = *(result->parseTree);
            _parseTreeHash = result->parseTreeHash;
        }
    }

    void BuildFileParserNode::setStreamableType(uint32_t type) {
        streamableTypeId = type;
    }

    uint32_t BuildFileParserNode::typeId() const {
        return streamableTypeId;
    }

    void BuildFileParserNode::stream(IStreamer* streamer) {
        throw std::runtime_error("not implemented");
        Node::stream(streamer);
    }

    void BuildFileParserNode::prepareDeserialize() {
        throw std::runtime_error("not implemented");
        Node::prepareDeserialize();

    }
    void BuildFileParserNode::restore(void* context) {
        throw std::runtime_error("not implemented");
        Node::restore(context);
    }

}