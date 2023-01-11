#include "DirectoryNode.h"
#include "FileNode.h"
#include "ExecutionContext.h"

namespace
{
    using namespace YAM;

}
namespace YAM
{
    DirectoryNode::DirectoryNode(ExecutionContext* context, std::filesystem::path const& dirName)
        : Node(context, dirName)
    {}

    bool DirectoryNode::supportsPrerequisites() const {
        return false;
    }

    void DirectoryNode::getPrerequisites(std::vector<Node*>& prerequisites) const {
    }

    bool DirectoryNode::supportsOutputs() const {
        return false;
    }

    void DirectoryNode::getOutputs(std::vector<Node*>& outputs) const {
    }

    bool DirectoryNode::supportsInputs() const {
        return false;
    }

    void DirectoryNode::getInputs(std::vector<Node*>& inputs) const {
    }

    XXH64_hash_t DirectoryNode::getHash() const {
        return _hash;
    }

    void DirectoryNode::getFiles(std::vector<FileNode*>& filesInDir) {
        for (auto it = _content.begin(); it != _content.end(); ++it) {
            FileNode* n = dynamic_cast<FileNode*>(it->second);
            if (n != nullptr) {
                filesInDir.push_back(n);
            }
        }
    }

    void DirectoryNode::getSubDirs(std::vector<DirectoryNode*>& subDirsInDir) {
        for (auto it = _content.begin(); it != _content.end(); ++it) {
            DirectoryNode* n = dynamic_cast<DirectoryNode*>(it->second);
            if (n != nullptr) {
                subDirsInDir.push_back(n);
            }
        }
    }

    std::map<std::filesystem::path, Node*> const& DirectoryNode::getContent() {
        return _content;
    }

    // Note that pendingStartSelf is only called during node execution, i.e. when
    // state() == State::Executing. A node execution only starts when the node was
    // Dirty, i.e. when it is not sure that previously computed hash is still
    // up-to-date. Therefore always return true.
    bool DirectoryNode::pendingStartSelf() const {
        return true;
    }

    void DirectoryNode::startSelf() {
        auto d = Delegate<void>::CreateLambda([this]() { execute(); });
        _context->threadPoolQueue().push(std::move(d));
    }

    // Sync _lastWriteTime with current directory state. 
    // Return whether it was changed since previous update.
    bool DirectoryNode::updateLastWriteTime() {
        std::error_code ec;
        auto lwt = std::filesystem::last_write_time(name(), ec);
        auto lwutc = decltype(lwt)::clock::to_utc(lwt);
        if (lwutc != _lastWriteTime) {
            _lastWriteTime = lwutc;
            return true;
        }
        return false;
    }

    Node* DirectoryNode::createNode(std::filesystem::directory_entry const& dirEntry) {
        Node* n = nullptr;
        if (dirEntry.is_directory()) {
            n = new DirectoryNode(context(), dirEntry.path());
        } else if (dirEntry.is_regular_file()) {
            n = new FileNode(context(), dirEntry.path());
        }
        return n;
    }

    void DirectoryNode::updateContent() {

        // TODO: 
        //     - add nodes to ExecutionContext
        //     - consider using ref-counting to do garbage collection
        //       In this scenario many command nodes can still reference
        //       a garbage file node. Ref count decreases as command nodes
        //       (that ref that file node) re-execute.
        //       A node that reaches ref-count 0 must be removed from context
        //       and deleted.
        //
        auto oldContent = _content;
        _content.clear();
        for (auto const& dirEntry : std::filesystem::directory_iterator{ name()}) {
            Node* child;
            auto const& path = dirEntry.path();
            if (oldContent.contains(path)) {
                child = oldContent[path];
                oldContent[path] = nullptr;
            } else {
                child = createNode(dirEntry);
            }
            if (child != nullptr) _content.insert({ child->name(), child });
        }
        for (auto const& pair : oldContent) {
            Node* garbage = pair.second;
            if (garbage != nullptr) {
                // TODO: make sure garbage node is no longer referenced by its
                // parents, then delete the node
                // Consider using ref-counting to do gargabe collection
                delete garbage;
            }
        }
    }

    void DirectoryNode::updateHash() {
        std::vector<XXH64_hash_t> hashes;
        for (auto const& pair : _content) {
            Node* n = pair.second;
            hashes.push_back(XXH64_string(n->name().string()));
        }
        _hash = XXH64(hashes.data(), sizeof(XXH64_hash_t) * hashes.size(), 0);
    }

    void DirectoryNode::execute() {
        if (updateLastWriteTime()) {
            updateContent();
            updateHash();
        }
        postCompletion(Node::State::Ok);
    }
}