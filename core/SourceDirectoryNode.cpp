#include "SourceDirectoryNode.h"
#include "SourceFileNode.h"
#include "CommandNode.h"
#include "ExecutionContext.h"
#include "FileRepository.h"
#include "DotIgnoreNode.h"

namespace
{
    using namespace YAM;
}
namespace YAM
{
    SourceDirectoryNode::SourceDirectoryNode(ExecutionContext* context, std::filesystem::path const& dirName)
        : Node(context, dirName)
        , _dotIgnoreNode(std::make_shared<DotIgnoreNode>(context, this))
    {
        _dotIgnoreNode->addPreParent(this);
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
    }

    bool SourceDirectoryNode::supportsInputs() const {
        return false;
    }

    void SourceDirectoryNode::getInputs(std::vector<std::shared_ptr<Node>>& inputs) const {
    }

    XXH64_hash_t SourceDirectoryNode::getHash() const {
        return _hash;
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

    void SourceDirectoryNode::startSelf() {
        auto d = Delegate<void>::CreateLambda([this]() { execute(); });
        _context->threadPoolQueue().push(std::move(d));
    }

    // Sync _lastWriteTime with current directory state. 
    // Return whether it was changed since previous update.
    bool SourceDirectoryNode::updateLastWriteTime() {
        std::error_code ec;
        auto lwt = std::filesystem::last_write_time(name(), ec);
        auto lwutc = decltype(lwt)::clock::to_utc(lwt);
        if (lwutc != _lastWriteTime) {
            _lastWriteTime = lwutc;
            return true;
        }
        return false;
    }

    std::shared_ptr<Node> SourceDirectoryNode::createNode(std::filesystem::directory_entry const& dirEntry) {
        std::shared_ptr<Node> n = nullptr;
        if (dirEntry.is_directory()) {
            n = std::make_shared<SourceDirectoryNode>(context(), dirEntry.path());
        } else if (dirEntry.is_regular_file()) {
            n = std::make_shared<SourceFileNode>(context(), dirEntry.path());
        }
        return n;
    }

    void SourceDirectoryNode::updateContent(
        std::filesystem::directory_entry const& dirEntry,
        std::map<std::filesystem::path, std::shared_ptr<Node>>& oldContent
    ) {
        auto const& path = dirEntry.path();
        if (path.filename() == "junk.txt") {
            bool stop = true;
        }
        if (!_dotIgnoreNode->ignore(path)) {
            std::shared_ptr<Node> child;
            if (oldContent.contains(path)) {
                child = oldContent[path];
                oldContent[path] = nullptr;
            } else {
                child = createNode(dirEntry);
                if (child != nullptr) {
                    child->addPostParent(this);
                    context()->nodes().add(child);
                }
            }
            if (child != nullptr) _content.insert({ child->name(), child });
        }
    }

    void SourceDirectoryNode::updateContent() {
        auto oldContent = _content;
        _content.clear();

        if (std::filesystem::exists(name())) {
            for (auto const& dirEntry : std::filesystem::directory_iterator{ name() }) {
                updateContent(dirEntry, oldContent);
            }
        }

        for (auto const& pair : oldContent) {
            std::shared_ptr<Node> child = pair.second;
            if (child != nullptr) {
                _removeChildRecursively(child);
            }
        }
    } 

    void SourceDirectoryNode::_removeChildRecursively(std::shared_ptr<Node> child) {
        child->removePostParent(this);
        // removeIfPresent because a parent directory may already have removed
        // this directory recursively
        context()->nodes().removeIfPresent(child);
        auto dirChild = dynamic_pointer_cast<SourceDirectoryNode>(child);
        if (dirChild != nullptr) {
            for (auto const& pair : dirChild->getContent()) {
                _removeChildRecursively(pair.second);
            }
        }
    }

    void SourceDirectoryNode::clear() {
        for (auto const& pair : _content) {
            std::shared_ptr<Node> child;
            if (child != nullptr) {
                _removeChildRecursively(child);
            }
        }
        _content.clear();
    }

    void SourceDirectoryNode::updateHash() {
        std::vector<XXH64_hash_t> hashes;
        for (auto const& pair : _content) {
            std::shared_ptr<Node> n = pair.second;
            hashes.push_back(XXH64_string(n->name().string()));
        }
        _hash = XXH64(hashes.data(), sizeof(XXH64_hash_t) * hashes.size(), 0);
    }

    void SourceDirectoryNode::execute() {
        bool success = true;
        try {
            if (updateLastWriteTime()) {
                context()->statistics().registerUpdatedDirectory(this);
                updateContent();
                updateHash();
                LogRecord error(LogRecord::Aspect::Progress, std::string("Rehashed directory ").append(name().string()));
                context()->addToLogBook(error);
            }
        } catch (std::filesystem::filesystem_error) {
            success = false;
        }
        postSelfCompletion(success ? Node::State::Ok : Node::State::Failed);
    }
}