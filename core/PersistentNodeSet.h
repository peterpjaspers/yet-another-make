#pragma once

#include "NodeSet.h"

#include <memory>
#include <filesystem>
#include <map>
#include <unordered_set>
#include <queue>

namespace YAM { class IValueStreamer; }

namespace Btree {
    class StreamingTree
    {
    public:
        StreamingTree(std::filesystem::path const& path) {}
        std::vector<uint64_t> keys() { return std::vector<uint64_t>(2); }
        YAM::IValueStreamer* retrieve(uint64_t key) { return nullptr; }
        YAM::IValueStreamer* insert(uint64_t key) { return nullptr; }
        void remove(uint64_t key) {  }
        void commit() {}
    };
}

namespace YAM
{
    class __declspec(dllexport) PersistentNodeSet
    {
    public:
        typedef uint64_t Key;

        PersistentNodeSet(std::filesystem::path const& nodesFile);

        // If a transaction is in progress: abort it and rollback storage 
        // to last committed state. 
        // Return the nodes in last committed state.
        void rollback(NodeSet& nodes);

        std::shared_ptr<Node> retrieve(Key key);

        // If node not yet in storage: add it, else update it.
        Key insert(std::shared_ptr<Node> const& node);

        // If node is in storage: remove it.
        void remove(std::shared_ptr<Node> const& node);

        // Atomically commit the insert and remove requests made since 
        // previous commit to persistent storage.
        // Note: the first insert/remove request since previous commit starts 
        // a transaction.
        void commit();

    private:
        void abort();
        void retrieveNodes();

        Key allocateKey(Node* node);

        void retrieveKey(Key key);
        void processRetrieveQueue();

        void insertKey(Key key);
        void processInsertQueue();

        std::filesystem::path _file;
        std::shared_ptr<Btree::StreamingTree> _btree;
        std::map<uint8_t, std::shared_ptr<Btree::StreamingTree>> _typeToTree;
        uint64_t _nextId;
        std::map<Key, std::shared_ptr<Node>> _keyToNode;
        std::map<Node*, Key> _nodeToKey;
        uint32_t _retrieveNesting;
        uint32_t _insertNesting;
        std::queue<Key> _retrieveQueue;
        std::queue<Key> _insertQueue;
    };

}