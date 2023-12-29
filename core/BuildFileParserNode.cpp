#include "BuildFileParserNode.h"
#include "DirectoryNode.h"
#include "SourceFileNode.h"
#include "ExecutionContext.h"
#include "FileSystem.h"
#include "FileAspect.h"
#include "BuildFileParser.h"
#include "Globber.h"
#include "AcyclicTrail.h"
#include "IStreamer.h"

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
        , _parseTreeHash(rand())
    {}

    void BuildFileParserNode::buildFile(std::shared_ptr<SourceFileNode> const& newFile) {
        if (_buildFile != newFile) {
            if (_buildFile != nullptr) {
                CommandNode::script("");
                CommandNode::workingDirectory(nullptr);
            }
            _buildFile = newFile;
            if (_buildFile != nullptr) {
                auto node = context()->nodes().find(_buildFile->name().parent_path());
                auto workingDir = dynamic_pointer_cast<DirectoryNode>(node);
                CommandNode::workingDirectory(workingDir);
                if (_buildFile->name().extension() != ".txt") {
                    // TODO: find interpreter needed to run the stript
                    // For now: only .bat files are supported.
                    std::string script = _buildFile->name().filename().string();
                    CommandNode::script(script);
                }
            } else {
                CommandNode::script("");
                CommandNode::workingDirectory(nullptr);
            }
            setState(Node::State::Dirty);
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

    std::vector<BuildFileParserNode const*> const& BuildFileParserNode::dependencies() const {
        if (state() != Node::State::Ok) throw std::runtime_error("illegal state");
        return _dependencies;
    }

    std::shared_ptr<CommandNode::PostProcessResult> BuildFileParserNode::postProcess(std::string const& stdOut) {
        auto result = std::make_shared<ParseResult>();
        result->newState = Node::State::Ok;
        if (_buildFile != nullptr) {
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
        } else {
            result->parseTree = std::make_shared<BuildFile::File>();
            result->parseTreeHash = rand();
        }
        return result;
    }

    void BuildFileParserNode::commitPostProcessResult(std::shared_ptr<CommandNode::PostProcessResult>& sresult) {
        _dependencies.clear();
        auto result = dynamic_pointer_cast<ParseResult>(sresult);
        if (result->newState != Node::State::Ok) {
            if (!result->parseErrors.empty()) {
                LogRecord error(LogRecord::Error, result->parseErrors);
                context()->logBook()->add(error);
            }
        } else if (_buildFile != nullptr) {
            _parseTree = *(result->parseTree);
            _parseTreeHash = result->parseTreeHash;
            composeDependencies();
        } else {
            _parseTree = *(result->parseTree);
            _parseTreeHash = result->parseTreeHash;
        }
    }

    void BuildFileParserNode::composeDependencies() {
        _dependencies.clear();
        std::filesystem::path buildFileDirName = _buildFile->name().parent_path();
        auto node = context()->nodes().find(buildFileDirName);
        auto baseDir = dynamic_pointer_cast<DirectoryNode>(node);
        if (baseDir == nullptr) {
            throw std::runtime_error("No such directory: " + buildFileDirName.string());
        }
        for (auto const& buildFile : _parseTree.deps.depBuildFiles) {
            auto dep = findDependency(baseDir, buildFile);
            _dependencies.push_back(dep);
        }
    }
    
    BuildFileParserNode* BuildFileParserNode::findDependency(
        std::shared_ptr<DirectoryNode> const& baseDir,
        std::filesystem::path const& buildFileOrDirPath
    ) {
        auto buildFileDir = baseDir;
        auto optimizedPath = buildFileOrDirPath;
        Globber::optimize(buildFileDir, optimizedPath);
        auto bfDirNode = dynamic_pointer_cast<DirectoryNode>(context()->nodes().find(buildFileDir->name()));
        if (bfDirNode == nullptr) {
            throw std::runtime_error("No such directory: " + buildFileDir->name().string());
        }
        std::shared_ptr<BuildFileParserNode> bfpn = bfDirNode->buildFileParserNode();
        if (bfpn == nullptr) {
            throw std::runtime_error("No buildfile found in directory " + buildFileDir->name().string());
        }
        return bfpn.get();
    }

    bool BuildFileParserNode::walkDependencies(AcyclicTrail<BuildFileParserNode const*>& trail) const {
        if (state() != Node::State::Ok) throw std::runtime_error("illegal state");
        if (!trail.add(this)) return false;
        for (auto const* dep : _dependencies) {
            if (!dep->walkDependencies(trail)) return false;
        }
        trail.remove(this);
        return true;
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
        streamer->stream(_buildFile);
        _parseTree.stream(streamer);
        streamer->stream(_parseTreeHash);
    }

    void BuildFileParserNode::prepareDeserialize() {
        Node::prepareDeserialize();
        _dependencies.clear();
    }

    void BuildFileParserNode::restore(void* context) {
        Node::restore(context);
        composeDependencies();
    }

}