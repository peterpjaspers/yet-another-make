#include "BuildFileParserNode.h"
#include "BuildFileDependenciesCompiler.h"
#include "FileRepositoryNode.h"
#include "FileExecSpecsNode.h"
#include "DirectoryNode.h"
#include "SourceFileNode.h"
#include "GeneratedFileNode.h"
#include "GlobNode.h"
#include "ExecutionContext.h"
#include "FileSystem.h"
#include "FileAspect.h"
#include "BuildFileParser.h"
#include "Globber.h"
#include "AcyclicTrail.h"
#include "IStreamer.h"
#include "NodeMapStreamer.h"
#include "computeMapsDifference.h"

namespace
{
    using namespace YAM; 

    uint32_t streamableTypeId = 0;
    std::map<std::filesystem::path, std::shared_ptr<FileNode>> emptyFileNodes;
    std::map<std::filesystem::path, std::shared_ptr<Node>> emptyNodes;
    std::map<std::filesystem::path, std::shared_ptr<GlobNode>> emptyGlobs;

    void subscribeInputFiles(
        std::map<std::filesystem::path, std::shared_ptr<FileNode>> const& nodes,
        bool subscribe,
        StateObserver* observer
    ) {
        for (auto const& pair : nodes) {
            auto const& fileNode = pair.second;
            auto const& genFileNode = dynamic_pointer_cast<GeneratedFileNode>(fileNode);
            if (genFileNode == nullptr) {
                if (subscribe) fileNode->addObserver(observer);
                else fileNode->removeObserver(observer);
            } else {
                if (subscribe) genFileNode->producer()->addObserver(observer);
                else genFileNode->producer()->removeObserver(observer);
            }
        }
    }

    void subscribeBuildFileGlobs(
        std::map<std::filesystem::path, std::shared_ptr<Node>> const& nodes,
        bool subscribe,
        StateObserver* observer
    ) {
        for (auto const& pair : nodes) {
            auto const& fileNode = pair.second;
            auto const& globNode = dynamic_pointer_cast<GlobNode>(fileNode);
            if (globNode != nullptr) {
                if (subscribe) fileNode->addObserver(observer);
                else fileNode->removeObserver(observer);
            }
        }
    }

    void subscribeInputGlobs(
        std::map<std::filesystem::path, std::shared_ptr<GlobNode>> const& nodes,
        bool subscribe,
        StateObserver* observer
    ) {
        for (auto const& pair : nodes) {
            auto const& node = pair.second;
            if (subscribe) node->addObserver(observer);
            else node->removeObserver(observer);
        }
    }
}

namespace YAM
{
    class Parser 
    {
    public:
        Parser(std::filesystem::path const& buildFile_)
            : buildFile(buildFile_)
        {}

        void process()  {
            try {
                BuildFileParser parser(buildFile);
                parseTree = parser.file();
                parseTreeHash = parseTree->computeHash();
            } catch (std::runtime_error ex) {
                parseErrors = ex.what();
            }
        }

        std::filesystem::path buildFile;
        std::string parseErrors;
        std::shared_ptr<BuildFile::File> parseTree;
        XXH64_hash_t parseTreeHash;
    };

}

namespace YAM
{

    BuildFileParserNode::BuildFileParserNode() {}

    BuildFileParserNode::BuildFileParserNode(ExecutionContext* context, std::filesystem::path const& name)
        : Node(context, name)
        , _buildFileHash(rand())
        , _executionHash(rand())
    {}

    void BuildFileParserNode::buildFile(std::shared_ptr<SourceFileNode> const& newFile) {
        if (_buildFile != newFile) {
            if (_buildFile != nullptr) {
                if (_buildFile->name().extension() == ".txt") {
                    _buildFile->removeObserver(this);
                } else {
                    _executor->removeObserver(this);
                    CommandNode::OutputNodes outputs = _executor->mandatoryOutputs();
                    _executor->outputFilters({}, {});
                    context()->nodes().remove(_executor);
                    for (auto const& pair : outputs) context()->nodes().remove(pair.second);
                    _executor = nullptr;
                }
                _dependencies.clear();
            }
            _buildFile = newFile;
            if (_buildFile != nullptr) {
                if (_buildFile->name().extension() == ".txt") {
                    _buildFile->addObserver(this);
                } else {
                    _executor = std::make_shared<CommandNode>(context(), _buildFile->name() / "__bfExecutor");
                    context()->nodes().add(_executor);
                    _executor->addObserver(this);
                    _executor->workingDirectory(buildFileDirectory());

                    std::filesystem::path srcBfDirPath = _buildFile->name().parent_path();
                    std::filesystem::path srcBfName = _buildFile->name().filename();
                    std::filesystem::path srcBfStem = srcBfName.stem().string();
                    std::filesystem::path genBfName = srcBfStem.string() + "_gen.txt";
                    std::filesystem::path genBfPath(srcBfDirPath / genBfName);
                    auto genNode = std::make_shared<GeneratedFileNode>(context(), genBfPath, _executor);
                    context()->nodes().add(genNode);
                    CommandNode::OutputFilter filter(CommandNode::OutputFilter::Output, genBfPath);
                    _executor->outputFilters({filter}, {genNode});

                    auto fileExecSpecsNode = repository()->fileExecSpecsNode();
                    std::string cmd = fileExecSpecsNode->command(srcBfName);
                    if (cmd.empty()) {
                        std::stringstream ss;
                        ss << "Cannot find the command needed to execute buildfile "
                           << _buildFile->absolutePath() << std::endl;
                        ss << "Fix this by adding an entry to file "
                           << fileExecSpecsNode->absoluteConfigFilePath()  << std::endl;
                        ss << "Fallback: the file will be executed as is." << std::endl;
                        LogRecord warning(LogRecord::Aspect::Warning, ss.str());
                        context()->addToLogBook(warning);
                        cmd = srcBfName.string();
                    }
                    std::string script = cmd + " > " + genBfName.string();
                    _executor->script(script);
                }
            }
            _parseTree = BuildFile::File();
            updateMap(context(), this, _buildFileDeps, emptyNodes);
            _dependencies.clear();
            _buildFileHash = rand();
            _executionHash = rand();
            modified(true);
            setState(Node::State::Dirty);
        }
    }

    std::shared_ptr<FileNode> BuildFileParserNode::fileToParse() const {
        std::shared_ptr<FileNode> buildFile;
        if (_executor != nullptr) {
            buildFile = _executor->mandatoryOutputs().begin()->second;
        } else {
            buildFile = _buildFile;
        }
        return buildFile;
    }

    std::shared_ptr<DirectoryNode> BuildFileParserNode::buildFileDirectory() const {
        auto node = context()->nodes().find(name().parent_path());
        return dynamic_pointer_cast<DirectoryNode>(node);
    }

    void BuildFileParserNode::start(PriorityClass prio) {
        Node::start(prio);
        if (_buildFile == nullptr) {
            postCompletion(Node::State::Ok);
        } else {
            std::vector<std::shared_ptr<Node>> requisites;
            if (_executor == nullptr) {
                requisites.push_back(_buildFile);
            } else {
                requisites.push_back(_executor);
            }
            auto callback = Delegate<void, Node::State>::CreateLambda(
                [this](Node::State state) { handleRequisitesCompletion(state); }
            );
            startNodes(requisites, std::move(callback), prio);
        }
    }

    void BuildFileParserNode::handleRequisitesCompletion(Node::State state) {
        if (state != Node::State::Ok) {
            notifyParseCompletion(state);
        } else {
            std::shared_ptr<FileNode> const& buildFile = fileToParse();
            XXH64_hash_t oldHash = _buildFileHash;
            _buildFileHash = buildFile->hashOf(FileAspect::entireFileAspect().name());
            if (_buildFileHash == oldHash) {
                startGlobs();
            } else {
                context()->statistics().registerSelfExecuted(this);
                _parser = std::make_shared<Parser>(buildFile->absolutePath());
                auto parse = Delegate<void>::CreateLambda([this]() { 
                    _parser->process(); 
                     postParseCompletion();
                });
                context()->threadPoolQueue().push(parse, PriorityClass::High);
            }
        }
    }

    void BuildFileParserNode::postParseCompletion() {
        auto handleCompletion = Delegate<void>::CreateLambda([this]() {
            handleParseCompletion();
        });
        context()->mainThreadQueue().push(handleCompletion);
    }

    void BuildFileParserNode::handleParseCompletion() {
        if (!_parser->parseErrors.empty()) {
            LogRecord error(LogRecord::Error, _parser->parseErrors);
            context()->logBook()->add(error);
            notifyParseCompletion(Node::State::Failed);
        } else {
            try {
                BuildFileDependenciesCompiler compiler(
                    context(),
                    buildFileDirectory(),
                    *(_parser->parseTree),
                    BuildFileDependenciesCompiler::Mode::BuildFileDeps);
                updateMap(context(), this, _buildFileDeps, compiler.buildFiles());
                startGlobs();
            } catch (std::runtime_error e) {
                LogRecord error(LogRecord::Error, e.what());
                context()->logBook()->add(error);
                notifyParseCompletion(Node::State::Failed);
            }
        }
    }

    void BuildFileParserNode::startGlobs() {
        std::vector<std::shared_ptr<Node>> globs;
        for (auto const& pair : _buildFileDeps) {
            auto globNode = dynamic_pointer_cast<GlobNode>(pair.second);
            if (globNode != nullptr) globs.push_back(pair.second);
        }
        if (globs.empty()) {
            handleGlobsCompletion(Node::State::Ok);
        } else {
            auto callback = Delegate<void, Node::State>::CreateLambda(
                [this](Node::State state) { handleGlobsCompletion(state); }
            );
            startNodes(globs, std::move(callback), PriorityClass::VeryHigh);
        }
    }

    void BuildFileParserNode::handleGlobsCompletion(Node::State state) {
        if (state != Node::State::Ok) {
            notifyParseCompletion(state);
        } else {
            if (composeDependencies()) {
                notifyParseCompletion(Node::State::Ok);
            } else {
                notifyParseCompletion(Node::State::Failed);
            }
        }
    }

    void BuildFileParserNode::notifyParseCompletion(Node::State newState) {
        if (newState == Node::State::Ok) {
            bool changedParseTree = false;
            if (_parser != nullptr) {
                changedParseTree = _parser->parseTreeHash != _parseTree.computeHash();
                _parseTree = *(_parser->parseTree);
            }
            bool logChanges = context()->logBook()->mustLogAspect(LogRecord::Aspect::FileChanges);
            if (logChanges && changedParseTree) {
                std::stringstream ss;
                ss << className() << " " << name().string();
                if (changedParseTree) {
                    ss << " has changed parse tree .";
                }
                LogRecord change(LogRecord::FileChanges, ss.str());
                context()->logBook()->add(change);
            }
            auto prevHash = _executionHash;
            _executionHash = computeExecutionHash();
            if (prevHash != _executionHash && logChanges) {
                std::stringstream ss;
                ss << className() << " " << name().string();
                ss << " has changed parse tree and/or changed list of buildfile dependencies.";
                LogRecord change(LogRecord::FileChanges, ss.str());
                context()->logBook()->add(change);
            }
            if (!modified()) modified(prevHash != _executionHash);
        } else {
            _executionHash = rand();
            updateMap(context(), this, _buildFileDeps, emptyNodes);
            _dependencies.clear(); 
            modified(true);
        }
        _parser = nullptr;
        Node::notifyCompletion(newState);
    }


    XXH64_hash_t BuildFileParserNode::computeExecutionHash() const {
        std::vector<XXH64_hash_t> hashes;
        hashes.push_back(_parseTree.computeHash());
        for (auto const& pair : _buildFileDeps) {
            auto const& glob = dynamic_pointer_cast<GlobNode>(pair.second);
            if (glob != nullptr) hashes.push_back(glob->executionHash());
        }
        hashes.push_back(repository()->hash());
        XXH64_hash_t hash = XXH64(hashes.data(), sizeof(XXH64_hash_t) * hashes.size(), 0);
        return hash;
    }

    std::shared_ptr<SourceFileNode> BuildFileParserNode::buildFile() const {
        return _buildFile;
    }

    BuildFile::File const& BuildFileParserNode::parseTree() const {
        if (state() != Node::State::Ok) throw std::runtime_error("illegal state");
        return _parseTree;
    }

    XXH64_hash_t BuildFileParserNode::executionHash() const {
        if (state() != Node::State::Ok) throw std::runtime_error("illegal state");
        return _executionHash;
    }

    std::vector<BuildFileParserNode const*> const& BuildFileParserNode::dependencies() {
        if (_buildFile != nullptr) {
            if (state() != Node::State::Ok) throw std::runtime_error("illegal state");
            if (_dependencies.empty() && !_parseTree.deps.depBuildFiles.empty()) {
                composeDependencies();
            }
        }
        return _dependencies;
    }

    bool BuildFileParserNode::composeDependencies() {
        auto oldDeps = _dependencies;
        _dependencies.clear();
        std::unordered_set<BuildFileParserNode*> added;
        for (auto const& pair : _buildFileDeps) {
            auto globNode = dynamic_pointer_cast<GlobNode>(pair.second);
            if (globNode != nullptr) {
                for (auto const& match : globNode->matches()) {
                    BuildFileParserNode* dep = findDependency(match->name(), false);
                    if (dep != nullptr && added.insert(dep).second) {
                        _dependencies.push_back(dep);
                    }
                }
            } else {
                BuildFileParserNode* dep = findDependency(pair.second->name(), true);
                if (dep == nullptr) return false;
                _dependencies.push_back(dep);
            }
        }
        if (
            oldDeps != _dependencies && !_dependencies.empty()
            && context()->logBook()->mustLogAspect(LogRecord::Aspect::FileChanges)
        ) {
            std::stringstream ss;
            ss << "Buildfile " << _buildFile->name().string() << " depends on the following buildfiles:" << std::endl;
            for (auto parser : _dependencies) {
                ss << "\t" << parser->_buildFile->name().string() << std::endl;
            }
            LogRecord change(LogRecord::Aspect::FileChanges, ss.str());
            context()->logBook()->add(change);
        }
        return true;
    }
    
    BuildFileParserNode* BuildFileParserNode::findDependency(
        std::filesystem::path const& buildFileOrDirPath,
        bool requireBuildFile
    ) {
        std::shared_ptr<BuildFileParserNode> bfpn;
        auto node = context()->nodes().find(buildFileOrDirPath);
        auto dirNode = dynamic_pointer_cast<DirectoryNode>(node);
        auto fileNode = dynamic_pointer_cast<SourceFileNode>(node);
        if (dirNode == nullptr && fileNode != nullptr) {
            auto dirPath = fileNode->name().parent_path();
            dirNode = dynamic_pointer_cast<DirectoryNode>(context()->nodes().find(dirPath));
        }
        if (dirNode != nullptr && dirNode->repository()->repoType() == FileRepositoryNode::RepoType::Build) {
            bfpn = dirNode->buildFileParserNode();
            if (bfpn == nullptr) {
                if (fileNode == nullptr && requireBuildFile) {
                    LogRecord error(LogRecord::Error, "No buildfile found in directory: " + dirNode->name().string());
                    context()->logBook()->add(error);
                    bfpn = nullptr;
                }
            } else if (fileNode != nullptr && fileNode != bfpn->buildFile() && requireBuildFile) {
                LogRecord error(LogRecord::Error, "Not a buildfile: " + buildFileOrDirPath.string());
                context()->logBook()->add(error);
                bfpn = nullptr;
            }
        } else if (requireBuildFile) {
            LogRecord error(LogRecord::Error, "No such file or directory: " + buildFileOrDirPath.string());
            context()->logBook()->add(error);
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
        Node::stream(streamer);
        streamer->stream(_buildFile);
        streamer->stream(_executor);
        streamer->stream(_buildFileHash);
        _parseTree.stream(streamer);
        NodeMapStreamer::stream(streamer, _buildFileDeps);
        streamer->stream(_executionHash);
    }

    void BuildFileParserNode::prepareDeserialize() {
        Node::prepareDeserialize();
        if (_buildFile != nullptr && _buildFile == fileToParse()) {
            _buildFile->removeObserver(this);
        }
        if (_executor != nullptr) _executor->removeObserver(this);
        _dependencies.clear();
        _parseTree = BuildFile::File();
        subscribeBuildFileGlobs(_buildFileDeps, false, this);
        _buildFileDeps.clear();
    }

    bool BuildFileParserNode::restore(void* context, std::unordered_set<IPersistable const*>& restored)  {
        if (!Node::restore(context, restored)) return false;
        if (_executor != nullptr) {
            _executor->restore(context, restored);
            _executor->addObserver(this);
        }
        if (_buildFile != nullptr) {
            _buildFile->restore(context, restored);
            if (_buildFile == fileToParse()) {
                _buildFile->addObserver(this);
            }
        }
        NodeMapStreamer::restore(_buildFileDeps);
        subscribeBuildFileGlobs(_buildFileDeps, true, this);
        return true;
    }
}