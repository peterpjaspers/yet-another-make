#include "DirectoryNode.h"
#include "FileRepository.h"
#include "SourceFileNode.h"
#include "GeneratedFileNode.h"
#include "ExecutionContext.h"
#include "DotIgnoreNode.h"
#include "BuildFileParserNode.h"
#include "BuildFileCompilerNode.h"
#include "IStreamer.h"

#include <regex>

namespace
{
    using namespace YAM;
    uint32_t streamableTypeId = 0;

    auto minLwt = std::chrono::time_point<std::chrono::utc_clock>::min();

    std::shared_ptr<Node> createNode(
        DirectoryNode* parent,
        std::filesystem::directory_entry const& dirEntry, 
        std::filesystem::path const& name,
        ExecutionContext* context
    ) {
        std::shared_ptr<Node> node = nullptr;
        if (dirEntry.is_directory()) {
            node = std::make_shared<DirectoryNode>(context, name, parent);
        } else if (dirEntry.is_regular_file()) {
            node = std::make_shared<SourceFileNode>(context, name);
        } else {
            // bool notHandled = true;
        }
        return node;
    }

    std::shared_ptr<SourceFileNode> findBuildFile(
        std::map<std::filesystem::path,
        std::shared_ptr<Node>> content
    ) {
        static std::regex fileNameRe(R"(^buildfile_yam\..*$)");
        for (auto const& pair : content) {
            auto fileName = pair.first.filename();
            if (std::regex_match(fileName.string(), fileNameRe)) {
                auto buildFileNode = dynamic_pointer_cast<SourceFileNode>(pair.second);
                return buildFileNode;
            }
        }
        return nullptr;
    }
}

namespace YAM
{
    DirectoryNode::DirectoryNode(
        ExecutionContext* context, 
        std::filesystem::path const& dirName,
        DirectoryNode* parent)
        : Node(context, dirName)
        , _parent(parent)
        , _dotIgnoreNode(std::make_shared<DotIgnoreNode>(context, dirName / ".dotignore", this))
        , _executionHash(rand())
    {}

    void DirectoryNode::addPrerequisitesToContext() {
        context()->nodes().add(_dotIgnoreNode);
        _dotIgnoreNode->addObserver(this);
        _dotIgnoreNode->addPrerequisitesToContext();
    }
    
    DirectoryNode::~DirectoryNode() {
    }

    XXH64_hash_t DirectoryNode::executionHash() const {
        return _executionHash;
    }

    std::shared_ptr<DirectoryNode> DirectoryNode::parent() const { 
        return _parent == nullptr ? nullptr : _parent->shared_from_this();
    }

    void DirectoryNode::parent(DirectoryNode* parent) {
        _parent = parent;
    }

    void DirectoryNode::getFiles(std::vector<std::shared_ptr<FileNode>>& filesInDir) {
        filesInDir.clear();
        for (auto it = _content.begin(); it != _content.end(); ++it) {
            std::shared_ptr<FileNode> n = dynamic_pointer_cast<FileNode>(it->second);
            if (n != nullptr) {
                filesInDir.push_back(n);
            }
        }
    }

    void DirectoryNode::getSubDirs(std::vector<std::shared_ptr<DirectoryNode>>& subDirsInDir) {
        subDirsInDir.clear();
        for (auto it = _content.begin(); it != _content.end(); ++it) {
            std::shared_ptr<DirectoryNode> n = dynamic_pointer_cast<DirectoryNode>(it->second);
            if (n != nullptr) {
                subDirsInDir.push_back(n);
            }
        }
    }

    void DirectoryNode::getOutputs(std::vector<std::shared_ptr<Node>>& outputs) const {
        for (auto const& pair : _content) outputs.push_back(pair.second);
    }

    void DirectoryNode::getInputs(std::vector<std::shared_ptr<Node>>& inputs) const {
        inputs.push_back(dynamic_pointer_cast<Node>(_dotIgnoreNode));
    }

    std::shared_ptr<Node> DirectoryNode::findChild(
        std::shared_ptr<DirectoryNode> directory,
        std::filesystem::path::iterator pit,
        std::filesystem::path::iterator pitEnd
     ) {
        static std::filesystem::path up("..");
        static std::filesystem::path stay(".");

        std::shared_ptr<Node> child;
        if (pit == pitEnd) return child;

        if (*pit == up) {
            pit++;
            auto pdir = directory->parent();
            if (pit == pitEnd) {
                child = pdir;
            } else if (pdir != nullptr) {
                child = findChild(pdir, pit, pitEnd);
            }
        } else if (*pit == stay) {
            pit++;
            if (pit == pitEnd) {
                child = directory;
            } else {
                child = findChild(directory, pit, pitEnd);
            }
        } else {
            auto cit = directory->_content.find(directory->name() / *pit);
            if (cit != directory->_content.end()) {
                pit++;
                if (pit == pitEnd) {
                    child = cit->second;
                } else {
                    auto cdir = dynamic_pointer_cast<DirectoryNode>(cit->second);
                    if (cdir != nullptr) {
                        child = findChild(cdir, pit, pitEnd);
                    } else {
                        child = cit->second;
                    }
                }
            }
        }
        return child;
    }

    std::shared_ptr<Node> DirectoryNode::findChild(std::filesystem::path const& path) {
        return findChild(shared_from_this(), path.begin(), path.end());
    }

    std::chrono::time_point<std::chrono::utc_clock> const& DirectoryNode::lastWriteTime() {
        return _lastWriteTime;
    }

    std::chrono::time_point<std::chrono::utc_clock> DirectoryNode::retrieveLastWriteTime() const {
        std::error_code ec;
        auto flwt = std::filesystem::last_write_time(absolutePath(), ec);
        auto ulwt = decltype(flwt)::clock::to_utc(flwt);
        return ulwt;
    }

    std::shared_ptr<Node> DirectoryNode::getNode(
        std::filesystem::directory_entry const& dirEntry,
        std::shared_ptr<FileRepository> const& repo,
        std::unordered_set<std::shared_ptr<Node>>& added,
        std::unordered_set<std::shared_ptr<Node>>& kept
    ) {
        std::shared_ptr<Node> child = nullptr;
        auto const& absPath = dirEntry.path();
        if (!_dotIgnoreNode->ignore(absPath)) {
            auto symPath = repo->symbolicPathOf(absPath);
            auto it = _content.find(symPath);
            if (it != _content.end()) {
                child = it->second;
                kept.insert(it->second);
            } else {
                // A node for this entry may be present in buildstate (contect->nodes()).
                // getNode() executes in threadpool context, hence buildstate access
                // at this point is not allowed. Instead optimistically create a new node
                // and check in main thread (commitResult) whether it already existed in
                // buildstate.
                child = createNode(this, dirEntry, symPath, context());
                added.insert(child);
            }
        }
        return child;
    }

    void DirectoryNode::retrieveContent(
        std::map<std::filesystem::path, std::shared_ptr<Node>>& content,
        std::unordered_set<std::shared_ptr<Node>>& added,
        std::unordered_set<std::shared_ptr<Node>>& removed,
        std::unordered_set<std::shared_ptr<Node>>& kept
    ) {
        auto repo = repository();
        std::filesystem::path absDir = repo->absolutePathOf(name());
        if (std::filesystem::exists(absDir)) {
            std::shared_ptr<Node> child = nullptr;
            for (auto const& dirEntry : std::filesystem::directory_iterator{ absDir }) {
                child = getNode(dirEntry, repo, added, kept);
                if (child != nullptr) content.insert({ child->name(), child });
            }
        }
        for (auto const& pair : _content) {
            if (!kept.contains(pair.second)) {
                removed.insert(pair.second);
            }
        }
    } 

    void DirectoryNode::_removeChildRecursively(std::shared_ptr<Node> const& child) {
        child->removeObserver(this);
        // removeIfPresent because a parent directory may already have removed
        // this directory recursively
        context()->nodes().removeIfPresent(child);
        auto dirChild = dynamic_pointer_cast<DirectoryNode>(child);
        if (dirChild != nullptr) {
            dirChild->clear();
        }
    }

    void DirectoryNode::clear() {
        if (_dotIgnoreNode != nullptr) {
            _dotIgnoreNode->clear();
            _dotIgnoreNode->removeObserver(this);
            context()->nodes().remove(_dotIgnoreNode);
            _dotIgnoreNode = nullptr;
        }
        for (auto const& pair : _content) {
            std::shared_ptr<Node> const& child = pair.second;
            if (child != nullptr) {
                _removeChildRecursively(child);
            }
        }
        if (_buildFileParserNode != nullptr) {
            context()->nodes().remove(_buildFileParserNode);
            context()->nodes().remove(_buildFileCompilerNode);
        }
        _content.clear();
        modified(true);
    }

    XXH64_hash_t DirectoryNode::computeExecutionHash(
        XXH64_hash_t dotIgnoreNodeHash,
        std::map<std::filesystem::path, std::shared_ptr<Node>> const& content
    ) const {
        std::vector<XXH64_hash_t> hashes;
        hashes.push_back(dotIgnoreNodeHash);
        for (auto const& pair : content) {
            std::shared_ptr<Node> n = pair.second;
            hashes.push_back(XXH64_string(n->name().string()));
        }
        return XXH64(hashes.data(), sizeof(XXH64_hash_t) * hashes.size(), 0);
    }

    void DirectoryNode::handleDirtyOf(Node* observedNode) {
        if (observedNode == _dotIgnoreNode.get()) {
            if (_dotIgnoreNode->state() != Node::State::Dirty) throw std::exception("Unexpected state of _dotIgnoreNode");
            setState(Node::State::Dirty);
        }
    }

    void DirectoryNode::start() {
        Node::start();
        std::vector<Node*> requisites;
        requisites.push_back(_dotIgnoreNode.get());
        auto callback = Delegate<void, Node::State>::CreateLambda(
            [this](Node::State s) { handleRequisitesCompletion(s); }
        );
        Node::startNodes(requisites, std::move(callback));
    }

    void DirectoryNode::handleRequisitesCompletion(Node::State state) {
        if (state != Node::State::Ok) {
            Node::notifyCompletion(state);
        } else if (canceling()) {
            Node::notifyCompletion(Node::State::Canceled);
        } else {
            context()->statistics().registerSelfExecuted(this);
            auto d = Delegate<void>::CreateLambda(
                [this]() { retrieveContentIfNeeded(); }
            );
            context()->threadPoolQueue().push(std::move(d));
        }
    }

    // Executes in a threadpool thread
    void DirectoryNode::retrieveContentIfNeeded() {
        auto result = std::make_shared<RetrieveResult>();
        bool success = true;
        try {
            result->_lastWriteTime = retrieveLastWriteTime();
            result->_executionHash = computeExecutionHash(_dotIgnoreNode->hash(), result->_content);
            if (
                result->_lastWriteTime != _lastWriteTime 
                || result->_executionHash != _executionHash // because _dotIgnoreNode changed
            ) {
                retrieveContent(result->_content, result->_added, result->_removed, result->_kept);
                result->_executionHash = computeExecutionHash(_dotIgnoreNode->hash(), result->_content);
            }
        } catch (std::filesystem::filesystem_error) {
            success = false;
        }
        result->_newState = success ? Node::State::Ok : Node::State::Failed;
        auto d = Delegate<void>::CreateLambda(
            [this, result]() { handleRetrieveContentCompletion(*result); }
        );
        context()->mainThreadQueue().push(std::move(d));
    }

    void DirectoryNode::handleRetrieveContentCompletion(RetrieveResult& result) {
        if (result._newState != Node::State::Ok) {
            notifyCompletion(result._newState);
        } else if (canceling()) {
            result._newState = Node::State::Canceled;
            notifyCompletion(result._newState);
        } else {
            if (
                result._lastWriteTime == _lastWriteTime
                && result._executionHash == _executionHash
            ) {
                notifyCompletion(result._newState);
            } else {
                commitResult(result);
                startSubDirs();
            }
        }
    }

    void DirectoryNode::startSubDirs() {
        std::vector<Node*> children;
        for (auto it = _content.begin(); it != _content.end(); ++it) {
            auto sdir = dynamic_pointer_cast<DirectoryNode>(it->second);
            if (sdir != nullptr) {
                children.push_back(sdir.get());
            }
        }
        auto callback = Delegate<void, Node::State>::CreateLambda(
            [this](Node::State s) { Node::notifyCompletion(s); }
        );
        Node::startNodes(children, std::move(callback));
    }

    // Executes in main thread
    void DirectoryNode::commitResult(YAM::DirectoryNode::RetrieveResult& result) {
        _lastWriteTime = result._lastWriteTime;
        _executionHash = result._executionHash;
        _content.clear();
        for (auto const& node : result._kept) {
            _content.insert({ node->name(), node });
        }
        for (auto const& n : result._added) {
            // A node with name n->name() may already exist in buildstate.
            // If so, use that one instead of n.
            auto node = context()->nodes().find(n->name());
            auto generatedNode = dynamic_pointer_cast<GeneratedFileNode>(node);
            if (generatedNode == nullptr) {
                if (node == nullptr) {
                    node = n;
                    context()->nodes().add(node);
                }
                node->addObserver(this);
                _content.insert({ node->name(), node });
                auto dir = dynamic_pointer_cast<DirectoryNode>(node);
                if (dir != nullptr) dir->addPrerequisitesToContext();
            }
        }
        for (auto const& n : result._removed) {
            _removeChildRecursively(n);
        }
        updateBuildFileParserNode();

        modified(true);
        context()->statistics().registerUpdatedDirectory(this);
        if (context()->logBook()->mustLogAspect(LogRecord::Aspect::DirectoryChanges)) {
            LogRecord progress(LogRecord::Aspect::Progress, std::string("Rehashed directory ").append(name().string()));
            context()->addToLogBook(progress);
        }
    }
    
    void DirectoryNode::updateBuildFileParserNode() {
        std::shared_ptr<SourceFileNode> buildFile = findBuildFile(_content);
        if (buildFile != nullptr) {
            if (_buildFileParserNode == nullptr) {
                std::filesystem::path bfpName(name() / "__bfParser");
                _buildFileParserNode = std::make_shared<BuildFileParserNode>(context(), bfpName);
                context()->nodes().add(_buildFileParserNode);
                std::filesystem::path bfcName(name() / "__bfCompiler");
                _buildFileCompilerNode = std::make_shared<BuildFileCompilerNode>(context(), bfcName);
                context()->nodes().add(_buildFileCompilerNode);
            }
            _buildFileParserNode->buildFile(buildFile);
            _buildFileCompilerNode->buildFileParser(_buildFileParserNode);
        } else {
            if (_buildFileParserNode != nullptr) {
                _buildFileParserNode->buildFile(nullptr);
                _buildFileCompilerNode->buildFileParser(nullptr);
            }
        }
    }

    void DirectoryNode::setStreamableType(uint32_t type) {
        streamableTypeId = type;
    }

    uint32_t DirectoryNode::typeId() const {
        return streamableTypeId;
    }

    void DirectoryNode::stream(IStreamer* streamer) {
        Node::stream(streamer);
        streamer->stream(_lastWriteTime);
        streamer->stream(_executionHash);
        streamer->stream(_dotIgnoreNode);
        std::vector<std::shared_ptr<Node>> nodes;
        if (streamer->writing()) {
            for (auto const& p : _content) nodes.push_back(p.second);
        }
        streamer->streamVector(nodes);
        if (streamer->reading()) {
            // Take care: when streaming nodes from persistent repository the
            // nodes are constructed but their members may not yet have been 
            // streamed. Use temporary names to build the map and update the 
            // map in restore();
            uint32_t index = 0;
            for (auto n : nodes) {
                std::stringstream ss; ss << index; index += 1;
                _content.insert({ std::filesystem::path(ss.str()), n });
            }
        }
    }

    void DirectoryNode::prepareDeserialize() {
        Node::prepareDeserialize();
        if (_dotIgnoreNode != nullptr) _dotIgnoreNode->removeObserver(this);
        for (auto const& p : _content) p.second->removeObserver(this);
        _dotIgnoreNode = nullptr;
        _content.clear();
    }

    void DirectoryNode::restore(void* context) {
        Node::restore(context);
        _dotIgnoreNode->directory(this);
        _dotIgnoreNode->addObserver(this);
        std::vector<std::shared_ptr<Node>> nodes;
        for (auto const& p : _content) {
            nodes.push_back(p.second);
            p.second->addObserver(this);
            auto dir = dynamic_cast<DirectoryNode*>(p.second.get());
            if (dir != nullptr) dir->parent(this);
        }
        _content.clear();
        for (auto const& n : nodes) _content.insert({ n->name(), n });
    }
}