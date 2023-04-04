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
#include <set>

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
            PersistentBuildState::Key key = _buildState.store(persistable);
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
        , _storeNesting(0)
    {
        for (auto tid : buildStateTypes.ids) {
            auto tree = createTree(_directory, tid);
            _typeToTree.insert({ tid, tree });
        }
    }

    void PersistentBuildState::retrieve() {
        reset();
        retrieveAll();
        NodeSet& nodes = _context->nodes();
        for (auto pair : _keyToObject) {
            auto object = pair.second;
            if (buildStateTypes.isNode(object.get())) {
                auto node = dynamic_pointer_cast<Node>(object);
                nodes.add(node);
            } else if (buildStateTypes.isFileRepo(object.get())) {
                auto repo = dynamic_pointer_cast<FileRepository>(object);
                _context->addRepository(repo);
            }
        }
    }

    void PersistentBuildState::reset() {
        _keyToObject.clear();
        _objectToKey.clear();
        _storeQueue = std::queue<Key>();
        _retrieveQueue = std::queue<Key>();
        _retrieveNesting = 0;
        _storeNesting = 0;
        _nextId = 1;
        _toInsert.clear();
        _toReplace.clear();
        _toRemove.clear();
        _toRemoveKeys.clear();
        _context->clearBuildState();
    }

    void PersistentBuildState::retrieveAll() {
        // First instantiate all objects...
        for (auto pair : _typeToTree) {
            uint8_t type = pair.first;
            auto tree = pair.second;
            auto keys = tree->keys();
            for (auto key : keys) {
                KeyCode code(key);
                if (code._id >= _nextId) _nextId = code._id + 1;
                std::shared_ptr<IPersistable> object(buildStateTypes.instantiate(code._type, _context));
                auto pair = _keyToObject.insert({ key, object });
                _objectToKey.insert({ object.get(), key });
            }
        }
        
        // ...then retrieve the objects from the btree.
        // This approach results in retrieval of objects in key order.
        // This achieves maximum btree retrieve performance.
        for (auto pair : _typeToTree) {
            uint8_t type = pair.first;
            auto tree = pair.second;
            auto keys = tree->keys();
            for (auto key : keys) {
                retrieveKey(key);
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
        KeyCode code(key);
        auto tree = _typeToTree[code._type];
        IValueStreamer* vReader = tree->retrieve(key);
        SharedPersistableReader snReader(*this);
        Streamer reader(vReader, &snReader);
        std::shared_ptr<IPersistable> object = _keyToObject[key];
        object.get()->stream(&reader);
        _retrieveNesting--;
    }

    PersistentBuildState::Key PersistentBuildState::allocateKey(IPersistable* object) {
        KeyCode code(_nextId, static_cast<uint8_t>(buildStateTypes.getTypeId(object)));
        _nextId += 1;
        return code._key;
    }

    std::size_t PersistentBuildState::store() {
        std::unordered_set<std::shared_ptr<IPersistable>> buildState;
        std::unordered_set<std::shared_ptr<IPersistable>> storedState;
        _context->getBuildState(buildState);
        getStoredState(storedState);
        _context->computeStorageNeed(buildState, storedState, _toInsert, _toReplace, _toRemove);

        // Insert/replace/remove the objecs in key order to achieve maximum
        // btree retrieve performance. TODO: to be confirmed by Phil
        std::set<Key> keysToStore;
        for (auto const& p : _toInsert) keysToStore.insert(bindToKey(p));
        for (auto const& p : _toReplace) keysToStore.insert(_objectToKey[p.get()]);
        for (auto const& p : _toRemove) {
            Key key = _objectToKey[p.get()];
            keysToStore.insert(key);
            _toRemoveKeys.insert({p.get(), key});
        }
        for (auto key : keysToStore) {
            auto object = _keyToObject[key];
            if (_toRemove.contains(object)) {
                remove(object);
            } else {
                store(object);
            }
        }
        std::size_t nStored = _toInsert.size() + _toReplace.size() + _toRemove.size();
        if (!commitBtrees()) {
            rollback(true);
            nStored = 0;
        }
        _toInsert.clear();
        _toReplace.clear();
        _toRemove.clear();
        _toRemoveKeys.clear();
        return nStored;
    }

    // TODO: insertions in _toInsert/_toReplace not needed when the store(object)
    // function is made private and friend of SharedPersistableWriter. In that case
    // storage can only be started by the store() function.
    PersistentBuildState::Key PersistentBuildState::bindToKey(std::shared_ptr<IPersistable> const& object) {
        Key key;
        auto it = _objectToKey.find(object.get());
        if (it == _objectToKey.end()) {
            // new node, insert it irrespective of its modified state.
            key = allocateKey(object.get());
            _objectToKey.insert({ object.get(), key });
            _keyToObject.insert({ key, object });
            object->modified(true);
            _toInsert.insert(object); 
        } else {
            key = it->second;
            if (object->modified() && !_toInsert.contains(object)) _toReplace.insert(object);
        }
        return key;
    }

    PersistentBuildState::Key PersistentBuildState::store(std::shared_ptr<IPersistable> const& object) {
        Key key = bindToKey(object);
        if (object->modified()) {
            _storeQueue.push(key);
            object->modified(false);
            processStoreQueue();
        }
        return key;
    }

    void PersistentBuildState::processStoreQueue() {
        while (_storeNesting == 0 && !_storeQueue.empty()) {
            Key key = _storeQueue.front();
            _storeQueue.pop();
            storeKey(key);
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

    bool PersistentBuildState::commitBtrees() {
        //_btree->commit();
        for (auto const& p : _typeToTree) {
            p.second->commit();
        }
        return true;
    }

    void PersistentBuildState::storeKey(Key key) {
        _storeNesting++;
        std::shared_ptr<IPersistable> object = _keyToObject[key];
        KeyCode code(key);
        auto tree = _typeToTree[code._type];
        bool insert = _toInsert.contains(object);
        bool replace = _toReplace.contains(object);
        if (!insert && !replace) throw std::exception("unknown key");
        IValueStreamer* vWriter = insert ? tree->insert(key) : tree->replace(key);
        SharedPersistableWriter snWriter(*this);
        Streamer writer(vWriter, &snWriter);
        object->stream(&writer);
        vWriter->close();
        _storeNesting--;
    }
    
    void PersistentBuildState::rollback(bool recoverBtrees) {
        if (recoverBtrees) {
            // recover btree
        }
        for (auto const& object : _toRemove) {
            if (recoverBtrees) {
                Key key = _toRemoveKeys[object.get()];
                _keyToObject.insert({ key, object });
                _objectToKey.insert({ object.get(), key });
            }
            auto node = dynamic_pointer_cast<Node>(object);
            auto repo = dynamic_pointer_cast<FileRepository>(object);
            if (node != nullptr) {
                _context->nodes().add(node);
            } else if (repo != nullptr) {
                _context->addRepository(repo);
            } else {
                throw std::exception("unknown object class");
            }
        }
        for (auto const& object : _toInsert) {
            if (recoverBtrees) {
                auto it = _objectToKey.find(object.get());
                if (it != _objectToKey.end()) {
                    Key key = it->second;
                    _keyToObject.erase(key);
                    _objectToKey.erase(object.get());
                }
            }
            auto node = dynamic_pointer_cast<Node>(object);
            auto repo = dynamic_pointer_cast<FileRepository>(object);
            if (node != nullptr) {
                _context->nodes().remove(node);
            } else if (repo != nullptr) {
                _context->removeRepository(repo->name());
            } else { 
                throw std::exception("unknown object class"); 
            }
        }
        for (auto const& object : _toReplace) {
            // Replaces object-in-place by re-streaming it from btree.
            Key key = _objectToKey[object.get()];
            object->prepareDeserialize();
            retrieveKey(key);
        }
        for (auto const& object : _toReplace) {
            object->restore(_context);
        }
    }

    void PersistentBuildState::rollback() {
        std::unordered_set<std::shared_ptr<IPersistable>> buildState;
        std::unordered_set<std::shared_ptr<IPersistable>> storedState;
        _context->getBuildState(buildState);
        getStoredState(storedState);
        _context->computeStorageNeed(buildState, storedState, _toInsert, _toReplace, _toRemove);
        rollback(false);
        _toInsert.clear();
        _toReplace.clear();
        _toRemove.clear();
    }

    void PersistentBuildState::getStoredState(std::unordered_set<std::shared_ptr<IPersistable>>& storedState) {
        for (auto const& p : _keyToObject) storedState.insert(p.second);
    }
}
