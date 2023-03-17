#include "PersistentNodeSet.h"
#include "SourceFileNode.h"
#include "GeneratedFileNode.h"
#include "SourceDirectoryNode.h"
#include "DotIgnoreNode.h"
#include "CommandNode.h"
#include "Streamer.h"
#include "ObjectStreamer.h"
#include "BinaryValueStreamer.h"
#include "ISharedObjectStreamer.h"

#include <memory>
#include <filesystem>

namespace
{
    // Class that allocates unique type ids to the node classes. 
    class NodeTypes
    {
    public:
        enum NodeTypeId {
            CommandNode = 1,
            DotIgnoreNode = 2,
            GeneratedFileNode = 3,
            SourceDirectoryNode = 4,
            SourceFileNode = 5
        };

        NodeTypes() {
            YAM::CommandNode::setStreamableType(static_cast<uint32_t>(CommandNode));
            YAM::DotIgnoreNode::setStreamableType(static_cast<uint32_t>(DotIgnoreNode));
            YAM::GeneratedFileNode::setStreamableType(static_cast<uint32_t>(GeneratedFileNode));
            YAM::SourceDirectoryNode::setStreamableType(static_cast<uint32_t>(SourceDirectoryNode));
            YAM::SourceFileNode::setStreamableType(static_cast<uint32_t>(SourceFileNode));
        }

        Node* instantiate(uint32_t typeId) {
            auto mtid = static_cast<NodeTypeId>(typeId);
            switch (mtid) {
            case NodeTypeId::CommandNode: return new YAM::CommandNode();
            case NodeTypeId::DotIgnoreNode: return new YAM::DotIgnoreNode;
            case NodeTypeId::GeneratedFileNode: return new YAM::GeneratedFileNode;
            case NodeTypeId::SourceDirectoryNode: return new YAM::SourceDirectoryNode;
            case NodeTypeId::SourceFileNode: return new YAM::SourceFileNode;
            default: throw std::exception("unknown node type");
            }
        }
    };        
    
    NodeTypes nodeTypes;

    class SharedNodeWriter : public YAM::ISharedObjectStreamer
    {
    public:
        SharedNodeWriter(YAM::PersistentNodeSet& nodeRepo)
            : _nodeRepo(nodeRepo)
        {}

        void stream(IStreamer* writer, std::shared_ptr<IStreamable>& object) {
            auto node = dynamic_pointer_cast<YAM::Node>(object);
            PersistentNodeSet::Key key = _nodeRepo.insert(node);
            writer->stream(key);
        }

    private:
        YAM::PersistentNodeSet& _nodeRepo;
    };

    class SharedNodeReader : public YAM::ISharedObjectStreamer
    {
    public:
        SharedNodeReader(YAM::PersistentNodeSet& nodeRepo)
            : _nodeRepo(nodeRepo)
        {}

        void stream(IStreamer* reader, std::shared_ptr<IStreamable>& object) {
            PersistentNodeSet::Key key;
            reader->stream(key);
            std::shared_ptr<YAM::Node> node = _nodeRepo.retrieve(key);
            object = dynamic_pointer_cast<IStreamable>(node);
        }

    private:
        YAM::PersistentNodeSet& _nodeRepo;
    };

    class KeyCode {
    public:
        KeyCode(PersistentNodeSet::Key key) 
            : _key(key)
            , _id(key & 0xfffffff)
            , _type(static_cast<uint8_t>(key >> 56))
        {}

        KeyCode(uint64_t id, uint8_t type)
            : _key((static_cast<uint64_t>(type) << 56) | id)
            , _id(id)
            , _type(type) 
        {
            const uint64_t maxId = static_cast < uint64_t>(1) << 56;
            if (id > maxId) throw std::exception("id out of bounds");
        }

        PersistentNodeSet::Key _key;
        uint64_t _id;
        uint8_t _type;
    };

    uint8_t getType(Node* node) {
        throw std::exception("TODO");
    }
}

namespace YAM
{
    PersistentNodeSet::PersistentNodeSet(std::filesystem::path const& nodesFile)
        : _file(nodesFile)
    {}

    void PersistentNodeSet::rollback(NodeSet& nodes) {
        abort();
        retrieveNodes();
        nodes.clear();
        for (auto pair : _keyToNode) {
            nodes.add(pair.second);
        }
    }

    void PersistentNodeSet::abort() {
        _keyToNode.clear();
        _nodeToKey.clear();
        _insertQueue = std::queue<Key>();
        _retrieveQueue = std::queue<Key>();
        _retrieveNesting = 0;
        _insertNesting = 0;
        _nextId = 1;
        //btree.recover();
    }

    void PersistentNodeSet::retrieveNodes() {
        for (auto pair : _typeToTree) {
            uint8_t type = pair.first;
            auto tree = pair.second;
            for (Key id : tree->keys()) {
                retrieve(id);
                if (id >= _nextId) _nextId = id + 1;
            }
        }
    }

    void PersistentNodeSet::processRetrieveQueue() {
        while (_retrieveNesting == 0 && !_retrieveQueue.empty()) {
            Key key = _retrieveQueue.back();
            _retrieveQueue.pop();
            retrieveKey(key);
        }
    }

    std::shared_ptr<Node> PersistentNodeSet::retrieve(Key key) {
        auto it = _keyToNode.find(key);
        if (it == _keyToNode.end()) {
            KeyCode code(key);
            std::shared_ptr<Node> node(nodeTypes.instantiate(code._type));
            _keyToNode.insert({ key, node });
            _nodeToKey.insert({ node.get(), key });
            _retrieveQueue.push(key);
            processRetrieveQueue();
        }
        return _keyToNode[key];
    }

    void PersistentNodeSet::retrieveKey(Key key) {
        _retrieveNesting++;
        std::shared_ptr<Node> node = _keyToNode[key];
        KeyCode code(key);
        auto tree = _typeToTree[code._type];
        IValueStreamer* vReader = tree->retrieve(code._id);
        SharedNodeReader snReader(*this);
        Streamer reader(vReader, &snReader);
        node.get()->stream(&reader);
        _retrieveNesting--;
    }

    PersistentNodeSet::Key PersistentNodeSet::allocateKey(Node* node) {
        KeyCode code(_nextId, static_cast<uint8_t>(node->typeId()));
        _nextId += 1;
        return code._key;
    }

    PersistentNodeSet::Key PersistentNodeSet::insert(std::shared_ptr<Node> const& node) {
        Key key;
        auto it = _nodeToKey.find(node.get());
        if (it == _nodeToKey.end()) {
            key = allocateKey(node.get());
            _nodeToKey.insert({ node.get(), key });
            _keyToNode.insert({ key, node });
            _insertQueue.push(key);
            processInsertQueue();
        } else {
            key = it->second;
            if (node->modified()) {
                _insertQueue.push(key);
                node->modified(false);
                processInsertQueue();
            }
        }
        return key;
    }

    void PersistentNodeSet::processInsertQueue() {
        while (_insertNesting == 0 && !_insertQueue.empty()) {
            Key key = _insertQueue.back();
            _insertQueue.pop();
            insertKey(key);
        }
    }

    void PersistentNodeSet::remove(std::shared_ptr<Node> const& node) {
        auto it = _nodeToKey.find(node.get());
        if (it != _nodeToKey.end()) {
            Key key = it->second;
            _keyToNode.erase(key);
            _nodeToKey.erase(node.get());
            KeyCode code(key);
            auto tree = _typeToTree[code._type];
            tree->remove(code._id);
        }
    }

    void PersistentNodeSet::commit() {
        _btree->commit();
    }

    void PersistentNodeSet::insertKey(Key key) {
        _insertNesting++;
        Node* node = _keyToNode[key].get();
        KeyCode code(key);
        auto tree = _typeToTree[code._type];
        IValueStreamer* vWriter = tree->insert(code._id);
        SharedNodeWriter snWriter(*this);
        Streamer writer(vWriter, &snWriter);
        node->stream(&writer);
        vWriter->close();
        _insertNesting--;
    }
}
