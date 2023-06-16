#include "SourceDirectoryNode.h"
#include "SourceFileNode.h"
#include "CommandNode.h"
#include "ExecutionContext.h"
#include "FileRepository.h"
#include "DotIgnoreNode.h"
#include "IStreamer.h"

namespace
{
    using namespace YAM;
    uint32_t streamableTypeId = 0;

    auto minLwt = std::chrono::time_point<std::chrono::utc_clock>::min();

    std::shared_ptr<Node> createNode(
        SourceDirectoryNode* parent,
        std::filesystem::directory_entry const& dirEntry, 
        ExecutionContext* context
    ) {
        std::shared_ptr<Node> node = nullptr;
        if (dirEntry.is_directory()) {
            node = std::make_shared<SourceDirectoryNode>(context, dirEntry.path(), parent);
        } else if (dirEntry.is_regular_file()) {
            node = std::make_shared<SourceFileNode>(context, dirEntry.path());
        } else {
            // bool notHandled = true;
        }
        return node;
    }
}

namespace YAM
{
    SourceDirectoryNode::SourceDirectoryNode(
        ExecutionContext* context, 
        std::filesystem::path const& dirName,
        SourceDirectoryNode* parent)
        : Node(context, dirName)
        , _parent(parent)
        , _dotIgnoreNode(std::make_shared<DotIgnoreNode>(context, dirName / ".dotignore", this))
        , _executionHash(rand())
    {}

    void SourceDirectoryNode::addPrerequisitesToContext() {
        context()->nodes().add(_dotIgnoreNode);
        _dotIgnoreNode->addDependant(this);
        _dotIgnoreNode->addPrerequisitesToContext();
    }

    bool SourceDirectoryNode::supportsPrerequisites() const {
        return true;
    }

    void SourceDirectoryNode::getPrerequisites(std::vector<std::shared_ptr<Node>>& prerequisites) const {
        prerequisites.push_back(_dotIgnoreNode);
    }

    bool SourceDirectoryNode::supportsPostrequisites() const {
        return true;
    }

    void SourceDirectoryNode::getPostrequisites(std::vector<std::shared_ptr<Node>>& postrequisites) const {
        for (auto const& pair : _content) postrequisites.push_back(pair.second);
    }

    bool SourceDirectoryNode::supportsOutputs() const {
        return false;
    }

    void SourceDirectoryNode::getOutputs(std::vector<std::shared_ptr<Node>>& outputs) const {
        for (auto it = _content.begin(); it != _content.end(); ++it) {
            outputs.push_back(it->second);
        }
    }

    bool SourceDirectoryNode::supportsInputs() const {
        return false;
    }

    void SourceDirectoryNode::getInputs(std::vector<std::shared_ptr<Node>>& inputs) const {
    }

    XXH64_hash_t SourceDirectoryNode::executionHash() const {
        return _executionHash;
    }

    SourceDirectoryNode* SourceDirectoryNode::parent() const { 
        return _parent;
    }

    void SourceDirectoryNode::parent(SourceDirectoryNode* parent) {
        _parent = parent;
    }

    void SourceDirectoryNode::getFiles(std::vector<std::shared_ptr<FileNode>>& filesInDir) {
        filesInDir.clear();
        for (auto it = _content.begin(); it != _content.end(); ++it) {
            std::shared_ptr<FileNode> n = dynamic_pointer_cast<FileNode>(it->second);
            if (n != nullptr) {
                filesInDir.push_back(n);
            }
        }
    }

    void SourceDirectoryNode::getSubDirs(std::vector<std::shared_ptr<SourceDirectoryNode>>& subDirsInDir) {
        subDirsInDir.clear();
        for (auto it = _content.begin(); it != _content.end(); ++it) {
            std::shared_ptr<SourceDirectoryNode> n = dynamic_pointer_cast<SourceDirectoryNode>(it->second);
            if (n != nullptr) {
                subDirsInDir.push_back(n);
            }
        }
    }

    std::map<std::filesystem::path, std::shared_ptr<Node>> const& SourceDirectoryNode::getContent() {
        return _content;
    }

    std::chrono::time_point<std::chrono::utc_clock> const& SourceDirectoryNode::lastWriteTime() {
        return _lastWriteTime;
    }

    // Note that pendingStartSelf is only called during node execution, i.e. when
    // state() == State::Executing. A node execution only starts when the node was
    // Dirty, i.e. when it is not sure that previously computed hash is still
    // up-to-date. Therefore always return true.
    bool SourceDirectoryNode::pendingStartSelf() const {
        return true;
    }

    std::chrono::time_point<std::chrono::utc_clock> SourceDirectoryNode::retrieveLastWriteTime() const {
        std::error_code ec;
        auto lwt = std::filesystem::last_write_time(name(), ec);
        if (ec) return minLwt;
        return decltype(lwt)::clock::to_utc(lwt);
    }

    std::shared_ptr<Node> SourceDirectoryNode::getNode(
        std::filesystem::directory_entry const& dirEntry,
        std::unordered_set<std::shared_ptr<Node>>& added,
        std::unordered_set<std::shared_ptr<Node>>& kept
    ) const {
        std::shared_ptr<Node> child = nullptr;
        auto const& path = dirEntry.path();
        if (!_dotIgnoreNode->ignore(path)) {
            auto it = _content.find(path);
            if (it != _content.end()) {
                child = it->second;
                kept.insert(it->second);
            } else {
                child = createNode(const_cast<SourceDirectoryNode*>(this), dirEntry, context());
                added.insert(child);
            }
        }
        return child;
    }

    void SourceDirectoryNode::retrieveContent(
        std::map<std::filesystem::path, std::shared_ptr<Node>>& content,
        std::unordered_set<std::shared_ptr<Node>>& added,
        std::unordered_set<std::shared_ptr<Node>>& removed,
        std::unordered_set<std::shared_ptr<Node>>& kept
    ) const {
        if (std::filesystem::exists(name())) {
            std::shared_ptr<Node> child = nullptr;
            for (auto const& dirEntry : std::filesystem::directory_iterator{ name() }) {
                child = getNode(dirEntry, added, kept);
                if (child != nullptr) content.insert({ child->name(), child });
            }
        }
        for (auto const& pair : _content) {
            if (!kept.contains(pair.second)) {
                removed.insert(pair.second);
            }
        }
    } 

    void SourceDirectoryNode::_removeChildRecursively(std::shared_ptr<Node> const& child) {
        child->removePostParent(this);
        // removeIfPresent because a parent directory may already have removed
        // this directory recursively
        context()->nodes().removeIfPresent(child);
        auto dirChild = dynamic_pointer_cast<SourceDirectoryNode>(child);
        if (dirChild != nullptr) {
            dirChild->clear();
        }
    }

    void SourceDirectoryNode::clear() {
        if (_dotIgnoreNode != nullptr) {
            _dotIgnoreNode->clear();
            _dotIgnoreNode->removeDependant(this);
            context()->nodes().remove(_dotIgnoreNode);
            _dotIgnoreNode = nullptr;
        }
        for (auto const& pair : _content) {
            std::shared_ptr<Node> const& child = pair.second;
            if (child != nullptr) {
                _removeChildRecursively(child);
            }
        }
        _content.clear();
        modified(true);
    }

    XXH64_hash_t SourceDirectoryNode::computeExecutionHash(std::map<std::filesystem::path, std::shared_ptr<Node>> const& content) const {
        std::vector<XXH64_hash_t> hashes;
        for (auto const& pair : content) {
            std::shared_ptr<Node> n = pair.second;
            hashes.push_back(XXH64_string(n->name().string()));
        }
        return XXH64(hashes.data(), sizeof(XXH64_hash_t) * hashes.size(), 0);
    }

    void SourceDirectoryNode::selfExecute(ExecutionResult* result) const {
        bool success = true;
        try {
            result->_content.clear();
            result->_added.clear();
            result->_removed.clear();
            result->_kept.clear();
            result->_lastWriteTime = retrieveLastWriteTime();
            if (result->_lastWriteTime != _lastWriteTime) {
                retrieveContent(result->_content, result->_added, result->_removed, result->_kept);
                result->_executionHash = computeExecutionHash(result->_content);
            }
        } catch (std::filesystem::filesystem_error) {
            success = false;
        }
        result->_newState = success ? Node::State::Ok : Node::State::Failed;
    }
    void SourceDirectoryNode::selfExecute() {
        auto result = std::make_shared<ExecutionResult>();
        selfExecute(result.get());
        postSelfCompletion(result);
    }

    void SourceDirectoryNode::commitSelfCompletion(SelfExecutionResult const* result) {
        if (result->_newState == Node::State::Ok) {
            auto r = dynamic_cast<ExecutionResult const*>(result);
            if (r->_lastWriteTime != _lastWriteTime) {
                _lastWriteTime = r->_lastWriteTime;
                _content = r->_content;
                _executionHash = r->_executionHash;
                for (auto const& n : r->_added) {
                    n->addPostParent(this);
                    _context->nodes().add(n);
                    auto const dir = dynamic_pointer_cast<SourceDirectoryNode>(n);
                    if (dir != nullptr) dir->addPrerequisitesToContext();
                }
                for (auto const& n : r->_removed) {
                    _removeChildRecursively(n);
                }
                modified(true);
                context()->statistics().registerUpdatedDirectory(this);
                if (_context->logBook()->mustLogAspect(LogRecord::Aspect::DirectoryChanges)) {
                    LogRecord error(LogRecord::Aspect::Progress, std::string("Rehashed directory ").append(name().string()));
                    context()->addToLogBook(error);
                }
            }
        }
    }

    void SourceDirectoryNode::setStreamableType(uint32_t type) {
        streamableTypeId = type;
    }

    uint32_t SourceDirectoryNode::typeId() const {
        return streamableTypeId;
    }

    void SourceDirectoryNode::stream(IStreamer* streamer) {
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

    void SourceDirectoryNode::prepareDeserialize() {
        Node::prepareDeserialize();
        if (_dotIgnoreNode != nullptr) _dotIgnoreNode->removeDependant(this);
        for (auto const& p : _content) {
            p.second->removePostParent(this);
        }
        _dotIgnoreNode = nullptr;
        _content.clear();
    }

    void SourceDirectoryNode::restore(void* context) {
        Node::restore(context);
        _dotIgnoreNode->directory(this);
        _dotIgnoreNode->addDependant(this);
        std::vector<std::shared_ptr<Node>> nodes;
        for (auto const& p : _content) {
            nodes.push_back(p.second);            
            p.second->addPostParent(this);
            auto dir = dynamic_cast<SourceDirectoryNode*>(p.second.get());
            if (dir != nullptr) dir->parent(this);
        }
        _content.clear();
        for (auto n : nodes) _content.insert({ n->name(), n });
    }
}