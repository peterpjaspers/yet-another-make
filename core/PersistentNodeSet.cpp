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
#include "ExecutionContext.h"

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

        uint32_t getTypeId(Node* node) {
            auto tid = static_cast<NodeTypeId>(node->typeId());
            switch (tid) {
            case NodeTypeId::CommandNode: return node->typeId();
            case NodeTypeId::DotIgnoreNode: return node->typeId();
            case NodeTypeId::GeneratedFileNode: return node->typeId();
            case NodeTypeId::SourceDirectoryNode: return node->typeId();
            case NodeTypeId::SourceFileNode: return node->typeId();
            default: throw std::exception("unknown node type");
            }            
        }

        Node* instantiate(uint32_t typeId, ExecutionContext* context) {
            auto mtid = static_cast<NodeTypeId>(typeId);
            switch (mtid) {
            case NodeTypeId::CommandNode: return new YAM::CommandNode();
            case NodeTypeId::DotIgnoreNode: return new YAM::DotIgnoreNode();
            case NodeTypeId::GeneratedFileNode: return new YAM::GeneratedFileNode();
            case NodeTypeId::SourceDirectoryNode: return new YAM::SourceDirectoryNode();
            case NodeTypeId::SourceFileNode: return new YAM::SourceFileNode();
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

    std::shared_ptr<Btree::StreamingTree> createTree(
        std::filesystem::path const& dir, 
        NodeTypes::NodeTypeId id
    ) {
        auto tid = static_cast<int32_t>(id);
        std::stringstream ss;
        ss << tid;
        return std::make_shared< Btree::StreamingTree>(dir / ss.str());
    }
}

namespace YAM
{
    PersistentNodeSet::PersistentNodeSet(
        std::filesystem::path const& directory,
        ExecutionContext* context
    )
        : _directory(directory)
        , _context(context)
        , _nextId(1)
        , _retrieveNesting(0)
        , _insertNesting(0)
    {
        auto tree = createTree(_directory, NodeTypes::NodeTypeId::CommandNode);
        uint8_t tid = static_cast<char8_t>(NodeTypes::NodeTypeId::CommandNode);
        _typeToTree.insert({ tid, tree });
        tree = createTree(_directory, NodeTypes::NodeTypeId::DotIgnoreNode);
        tid = static_cast<char8_t>(NodeTypes::NodeTypeId::DotIgnoreNode);
        _typeToTree.insert({ tid, tree });
        tree = createTree(_directory, NodeTypes::NodeTypeId::GeneratedFileNode);
        tid = static_cast<char8_t>(NodeTypes::NodeTypeId::GeneratedFileNode);
        _typeToTree.insert({ tid, tree });
        tree = createTree(_directory, NodeTypes::NodeTypeId::SourceDirectoryNode);
        tid = static_cast<char8_t>(NodeTypes::NodeTypeId::SourceDirectoryNode);
        _typeToTree.insert({ tid, tree });
        tree = createTree(_directory, NodeTypes::NodeTypeId::SourceFileNode);
        tid = static_cast<char8_t>(NodeTypes::NodeTypeId::SourceFileNode);
        _typeToTree.insert({ tid, tree });
    }

    void PersistentNodeSet::retrieve() {
        abort();
        retrieveNodes();
        NodeSet& nodes = _context->nodes();
        nodes.clear();
        for (auto pair : _keyToNode) {
            nodes.add(pair.second);
        }
        for (auto pair : _keyToNode) {
            pair.second->restore(_context);
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
            auto keys = tree->keys();
            for (Key key : keys) {
                KeyCode code(key);
                if (code._id >= _nextId) _nextId = code._id + 1;
                retrieve(key);
            }
        }
    }

    void PersistentNodeSet::processRetrieveQueue() {
        while (_retrieveNesting == 0 && !_retrieveQueue.empty()) {
            Key key = _retrieveQueue.front();
            _retrieveQueue.pop();
            retrieveKey(key);
        }
    }

    std::shared_ptr<Node> PersistentNodeSet::retrieve(Key key) {
        auto it = _keyToNode.find(key);
        if (it == _keyToNode.end()) {
            KeyCode code(key);
            std::shared_ptr<Node> node(nodeTypes.instantiate(code._type, _context));
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
        IValueStreamer* vReader = tree->retrieve(key);
        SharedNodeReader snReader(*this);
        Streamer reader(vReader, &snReader);
        node.get()->stream(&reader);
        _retrieveNesting--;
    }

    PersistentNodeSet::Key PersistentNodeSet::allocateKey(Node* node) {
        KeyCode code(_nextId, static_cast<uint8_t>(nodeTypes.getTypeId(node)));
        _nextId += 1;
        return code._key;
    }

    PersistentNodeSet::Key PersistentNodeSet::insert(std::shared_ptr<Node> const& node) {
        Key key;
        auto it = _nodeToKey.find(node.get());
        if (it == _nodeToKey.end()) {
            // new node, insert it irrespective of its modified state.
            key = allocateKey(node.get());
            _nodeToKey.insert({ node.get(), key });
            _keyToNode.insert({ key, node });
            _insertQueue.push(key);
            node->modified(false);
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
            Key key = _insertQueue.front();
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

    void PersistentNodeSet::store() {
        _context->nodes().foreach(
            Delegate<void, std::shared_ptr<Node> const&>::CreateLambda(
                [this](std::shared_ptr<Node> node) {this->insert(node);})
        );
    }

    void PersistentNodeSet::commit() {
        //_btree->commit();
        for (auto const& p : _typeToTree) {
            p.second->commit();
        }
    }

    void PersistentNodeSet::insertKey(Key key) {
        _insertNesting++;
        Node* node = _keyToNode[key].get();
        KeyCode code(key);
        auto tree = _typeToTree[code._type];
        IValueStreamer* vWriter = tree->insert(key);
        SharedNodeWriter snWriter(*this);
        Streamer writer(vWriter, &snWriter);
        node->stream(&writer);
        vWriter->close();
        _insertNesting--;
    }
}
