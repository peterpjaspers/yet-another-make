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

    std::vector<BuildFileParserNode const*> const& BuildFileParserNode::dependencies() {
        if (state() != Node::State::Ok) throw std::runtime_error("illegal state");
        if (_dependencies.empty() && !_parseTree.deps.depBuildFiles.empty()) {
            composeDependencies();
        }
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
            if (composeDependencies()) {
                auto prevHash = _parseTreeHash;
                _parseTreeHash = result->parseTreeHash;
                if (prevHash != _parseTreeHash && context()->logBook()->mustLogAspect(LogRecord::Aspect::FileChanges)) {
                    std::stringstream ss;
                    ss << className() << " " << name().string() << " has a changed parse tree.";
                    LogRecord change(LogRecord::FileChanges, ss.str());
                    context()->logBook()->add(change);
                }
            } else {
                result->newState = Node::State::Failed;
                _parseTreeHash = rand();
            }
        } else {
            _parseTree = *(result->parseTree);
            _parseTreeHash = result->parseTreeHash;
        }
        modified(true);
    }

    bool BuildFileParserNode::composeDependencies() {
        _dependencies.clear();
        if (_buildFile == nullptr) return true;
        for (auto const& buildFile : _parseTree.deps.depBuildFiles) {
            auto dep = findDependency(buildFile);
            if (dep == nullptr) return false;
            _dependencies.push_back(dep);
        }
        return true;
    }
    
    BuildFileParserNode* BuildFileParserNode::findDependency(
        std::filesystem::path const& buildFileOrDirPath
    ) {
        auto buildFileDir = CommandNode::workingDirectory();
        auto node = buildFileDir->findChild(buildFileOrDirPath);
        auto dirNode = dynamic_pointer_cast<DirectoryNode>(node);
        if (dirNode == nullptr) {
            auto fileNode = dynamic_pointer_cast<SourceFileNode>(node);
            if (fileNode != nullptr) {
                auto dirPath = fileNode->name().parent_path();
                dirNode = dynamic_pointer_cast<DirectoryNode>(context()->nodes().find(dirPath));
            }
        }
        if (dirNode == nullptr) {
            LogRecord error(LogRecord::Error, "No such directory or file: " + buildFileOrDirPath.string());
            context()->logBook()->add(error);
            return nullptr;
        }
        std::shared_ptr<BuildFileParserNode> bfpn = dirNode->buildFileParserNode();
        if (bfpn == nullptr) {
            LogRecord error(LogRecord::Error, "No buildfile found in directory: " + dirNode->name().string());
            context()->logBook()->add(error);
            return nullptr;
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
        CommandNode::stream(streamer);
        streamer->stream(_buildFile);
        _parseTree.stream(streamer);
        streamer->stream(_parseTreeHash);
    }

    void BuildFileParserNode::prepareDeserialize() {
        CommandNode::prepareDeserialize();
        _dependencies.clear();
    }

    bool BuildFileParserNode::restore(void* context, std::unordered_set<IPersistable const*>& restored)  {
        if (!CommandNode::restore(context, restored)) return false;
        if (_buildFile != nullptr) _buildFile->restore(context, restored);
        return true;
    }

}