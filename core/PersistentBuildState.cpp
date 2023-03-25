#include "PersistentBuildState.h"
#include "SourceFileNode.h"
#include "GeneratedFileNode.h"
#include "SourceDirectoryNode.h"
#include "DotIgnoreNode.h"
#include "CommandNode.h"
#include "Streamer.h"
#include "BinaryValueStreamer.h"
#include "ISharedObjectStreamer.h"
#include "ExecutionContext.h"
#include "NodeSet.h"
#include "FileRepository.h"
#include "SourceFileRepository.h"

#include <memory>
#include <filesystem>

namespace
{
    // Class that allocates unique type ids to the node classes. 
    class BuildStateTypes
    {
    public:
        enum TypeId : uint8_t {
            Min = 0,
            CommandNode = 1,
            DotIgnoreNode = 2,
            GeneratedFileNode = 3,
            SourceDirectoryNode = 4,
            SourceFileNode = 5,
            FileRepository = 6,
            SourceFileRepository = 7,
            Max = 8
        };
        std::vector<TypeId> ids;

        BuildStateTypes() {
            YAM::CommandNode::setStreamableType(static_cast<uint32_t>(CommandNode));
            YAM::DotIgnoreNode::setStreamableType(static_cast<uint32_t>(DotIgnoreNode));
            YAM::GeneratedFileNode::setStreamableType(static_cast<uint32_t>(GeneratedFileNode));
            YAM::SourceDirectoryNode::setStreamableType(static_cast<uint32_t>(SourceDirectoryNode));
            YAM::SourceFileNode::setStreamableType(static_cast<uint32_t>(SourceFileNode));
            YAM::FileRepository::setStreamableType(static_cast<uint32_t>(FileRepository));
            YAM::SourceFileRepository::setStreamableType(static_cast<uint32_t>(SourceFileRepository));

            ids.push_back(CommandNode);
            ids.push_back(DotIgnoreNode);
            ids.push_back(GeneratedFileNode);
            ids.push_back(SourceDirectoryNode);
            ids.push_back(SourceFileNode);
            ids.push_back(FileRepository);
            ids.push_back(SourceFileRepository);
        }

        bool isNode(IPersistable* object) {
            auto tid = object->typeId();
            return ((TypeId::Min < tid) && (tid < TypeId::FileRepository));
        }

        bool isFileRepo(IPersistable* object) {
            auto tid = object->typeId();
            return ((TypeId::FileRepository <= tid) && (tid < TypeId::Max));
        }

        uint32_t getTypeId(IPersistable* object) {
            auto tid = object->typeId();
            if ((TypeId::Min < tid) && (tid < TypeId::Max)) return tid;
            throw std::exception("unknown node type");
        }

        IPersistable* instantiate(uint32_t typeId, ExecutionContext* context) {
            auto tid = static_cast<TypeId>(typeId);
            switch (tid) {
            case TypeId::CommandNode: return new YAM::CommandNode();
            case TypeId::DotIgnoreNode: return new YAM::DotIgnoreNode();
            case TypeId::GeneratedFileNode: return new YAM::GeneratedFileNode();
            case TypeId::SourceDirectoryNode: return new YAM::SourceDirectoryNode();
            case TypeId::SourceFileNode: return new YAM::SourceFileNode();
            case TypeId::FileRepository: return new YAM::FileRepository();
            case TypeId::SourceFileRepository: return new YAM::SourceFileRepository();
            default: throw std::exception("unknown node type");
            }
        }
    };        
    
    BuildStateTypes buildStateTypes;

    class SharedPersistableWriter : public YAM::ISharedObjectStreamer
    {
    public:
        SharedPersistableWriter(YAM::PersistentBuildState& buildState)
            : _buildState(buildState)
        {}

        void stream(IStreamer* writer, std::shared_ptr<IStreamable>& object) {
            auto persistable = dynamic_pointer_cast<IPersistable>(object);
            PersistentBuildState::Key key = _buildState.insert(persistable);
            writer->stream(key);
        }

    private:
        YAM::PersistentBuildState& _buildState;
    };

    class SharedPersistableReader : public YAM::ISharedObjectStreamer
    {
    public:
        SharedPersistableReader(YAM::PersistentBuildState& buildState)
            : _buildState(buildState)
        {}

        void stream(IStreamer* reader, std::shared_ptr<IStreamable>& object) {
            PersistentBuildState::Key key;
            reader->stream(key);
            std::shared_ptr<YAM::IPersistable> node = _buildState.retrieve(key);
            object = dynamic_pointer_cast<IStreamable>(node);
        }

    private:
        YAM::PersistentBuildState& _buildState;
    };

    class KeyCode {
    public:
        const uint32_t typeBits = 8;
        const uint32_t idBits = 64 - typeBits;
        const uint64_t idMask = (1 << idBits) - 1;
        const uint64_t maxId = (static_cast <uint64_t>(1) << idBits) - 1;

        KeyCode(PersistentBuildState::Key key) 
            : _key(key)
            , _id(key & idMask)
            , _type(static_cast<uint8_t>(key >> idBits))
        {}

        KeyCode(uint64_t id, uint8_t type)
            : _key((static_cast<uint64_t>(type) << idBits) | id)
            , _id(id)
            , _type(type) 
        {
            if (id > maxId) throw std::exception("id out of bounds");
        }

        PersistentBuildState::Key _key;
        uint64_t _id;
        uint8_t _type;
    };

    uint8_t getType(Node* node) {
        throw std::exception("TODO");
    }

    std::shared_ptr<Btree::StreamingTree> createTree(
        std::filesystem::path const& dir, 
        BuildStateTypes::TypeId id
    ) {
        auto tid = static_cast<int32_t>(id);
        std::stringstream ss;
        ss << tid;
        return std::make_shared< Btree::StreamingTree>(dir / ss.str());
    }
}

namespace YAM
{
    PersistentBuildState::PersistentBuildState(
        std::filesystem::path const& directory,
        ExecutionContext* context
    )
        : _directory(directory)
        , _context(context)
        , _nextId(1)
        , _retrieveNesting(0)
        , _insertNesting(0)
    {
        for (auto tid : buildStateTypes.ids) {
            auto tree = createTree(_directory, tid);
            _typeToTree.insert({ tid, tree });
        }
    }

    void PersistentBuildState::retrieve() {
        abort();
        retrieveAll();
        _context->clearBuildState();
        NodeSet& nodes = _context->nodes();
        for (auto pair : _keyToObject) {
            auto object = pair.second;
            if (buildStateTypes.isNode(object.get())) {
                auto node = dynamic_pointer_cast<Node>(object);
                nodes.add(node);
            } else if(buildStateTypes.isFileRepo(object.get())) {
                auto repo = dynamic_pointer_cast<FileRepository>(object);
                _context->addRepository(repo);
            }
        }
    }

    void PersistentBuildState::abort() {
        _keyToObject.clear();
        _objectToKey.clear();
        _insertQueue = std::queue<Key>();
        _retrieveQueue = std::queue<Key>();
        _retrieveNesting = 0;
        _insertNesting = 0;
        _nextId = 1;
        _nStored = 0;
        //btree.recover();
    }

    void PersistentBuildState::retrieveAll() {
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
        for (auto pair : _keyToObject) {
            pair.second->restore(_context);
        }
    }

    void PersistentBuildState::processRetrieveQueue() {
        while (_retrieveNesting == 0 && !_retrieveQueue.empty()) {
            Key key = _retrieveQueue.front();
            _retrieveQueue.pop();
            retrieveKey(key);
        }
    }

    std::shared_ptr<IPersistable> PersistentBuildState::retrieve(Key key) {
        auto it = _keyToObject.find(key);
        if (it == _keyToObject.end()) {
            KeyCode code(key);
            std::shared_ptr<IPersistable> object(buildStateTypes.instantiate(code._type, _context));
            auto pair = _keyToObject.insert({ key, object });
            it = pair.first;
            _objectToKey.insert({ object.get(), key });
            _retrieveQueue.push(key);
            processRetrieveQueue();
        }
        return it->second;
    }

    void PersistentBuildState::retrieveKey(Key key) {
        _retrieveNesting++;
        std::shared_ptr<IPersistable> object = _keyToObject[key];
        KeyCode code(key);
        auto tree = _typeToTree[code._type];
        IValueStreamer* vReader = tree->retrieve(key);
        SharedPersistableReader snReader(*this);
        Streamer reader(vReader, &snReader);
        object.get()->stream(&reader);
        _retrieveNesting--;
    }

    PersistentBuildState::Key PersistentBuildState::allocateKey(IPersistable* object) {
        KeyCode code(_nextId, static_cast<uint8_t>(buildStateTypes.getTypeId(object)));
        _nextId += 1;
        return code._key;
    }

    PersistentBuildState::Key PersistentBuildState::insert(std::shared_ptr<IPersistable> const& object) {
        Key key;
        auto it = _objectToKey.find(object.get());
        if (it == _objectToKey.end()) {
            // new node, insert it irrespective of its modified state.
            key = allocateKey(object.get());
            _objectToKey.insert({ object.get(), key });
            _keyToObject.insert({ key, object });
            _insertQueue.push(key);
            object->modified(false);
            _nStored += 1;
            processInsertQueue();
        } else {
            key = it->second;
            if (object->modified()) {
                _insertQueue.push(key);
                object->modified(false);
                _nStored += 1;
                processInsertQueue();
            }
        }
        return key;
    }

    void PersistentBuildState::processInsertQueue() {
        while (_insertNesting == 0 && !_insertQueue.empty()) {
            Key key = _insertQueue.front();
            _insertQueue.pop();
            insertKey(key);
        }
    }

    void PersistentBuildState::remove(std::shared_ptr<IPersistable> const& object) {
        auto it = _objectToKey.find(object.get());
        if (it != _objectToKey.end()) {
            Key key = it->second;
            _keyToObject.erase(key);
            _objectToKey.erase(object.get());
            KeyCode code(key);
            auto tree = _typeToTree[code._type];
            tree->remove(code._id);
        }
    }

    void PersistentBuildState::store() {
        _context->nodes().foreach(
            Delegate<void, std::shared_ptr<Node> const&>::CreateLambda(
                [this](std::shared_ptr<Node> node) {this->insert(node); })
        );
        for (auto const& pair : _context->repositories()) {
            insert(pair.second);
        }
    }

    uint64_t PersistentBuildState::nStored() const {
        return _nStored;
    }

    void PersistentBuildState::commit() {
        //_btree->commit();
        for (auto const& p : _typeToTree) {
            p.second->commit();
        }
    }

    void PersistentBuildState::insertKey(Key key) {
        _insertNesting++;
        IPersistable* object = _keyToObject[key].get();
        KeyCode code(key);
        auto tree = _typeToTree[code._type];
        IValueStreamer* vWriter = tree->insert(key);
        SharedPersistableWriter snWriter(*this);
        Streamer writer(vWriter, &snWriter);
        object->stream(&writer);
        vWriter->close();
        _insertNesting--;
    }
}
