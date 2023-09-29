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

namespace YAM
{
    // Class that allocates unique type ids to the node classes. 
    class BuildStateTypes
    {
    public:
        enum TypeId : BTree::TreeIndex {
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

        IPersistable* instantiate(uint32_t typeId) {
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

    class ValueStreamer : public IValueStreamer {
    public:
        ValueStreamer(BTree::ValueReader<PersistentBuildState::Key>& streamer)
            : _streamer(streamer)
            , _writing(false)
        {}
        ValueStreamer(BTree::ValueWriter<PersistentBuildState::Key>&streamer)
            : _streamer(streamer)
            , _writing(true)
        {}

        // Inherited via IValueStreamer
        virtual bool writing() const override { return _writing; }
        virtual void stream(void* bytes, unsigned int nBytes) override {
            int8_t* values = reinterpret_cast<int8_t*>(bytes);
            for (unsigned int i = 0; i < nBytes; i++) {
                _streamer.stream(values[i]);
            }
        }
        virtual void stream(bool& v) override { _streamer.stream(v); }
        virtual void stream(float& v) override { _streamer.stream(v); }
        virtual void stream(double& v) override { _streamer.stream(v); }
        virtual void stream(int8_t& v) override { _streamer.stream(v); }
        virtual void stream(uint8_t& v) override { _streamer.stream(v); }
        virtual void stream(int16_t& v) override { _streamer.stream(v); }
        virtual void stream(uint16_t& v) override { _streamer.stream(v); }
        virtual void stream(int32_t& v) override { _streamer.stream(v); }
        virtual void stream(uint32_t& v) override { _streamer.stream(v); }
        virtual void stream(int64_t& v) override { _streamer.stream(v); }
        virtual void stream(uint64_t& v) override { _streamer.stream(v); }

    private:
        BTree::ValueStreamer<PersistentBuildState::Key>& _streamer;
        bool _writing;
    };


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
        const uint32_t maxType = (static_cast <uint32_t>(1) << 8) - 1;
        const uint64_t idMask = (static_cast <uint64_t>(1) << idBits) - 1;
        const uint64_t maxId = idMask;

        KeyCode(PersistentBuildState::Key key) 
            : _key(key)
            , _id(key & idMask)
            , _type(static_cast<BTree::TreeIndex>(key >> idBits))
        {
        }

        KeyCode(uint64_t id, uint32_t type)
            : _key((static_cast<uint64_t>(type) << idBits) | id)
            , _id(id)
            , _type(static_cast<BTree::TreeIndex>(type))
        {
            if (id > maxId) {
                throw std::exception("id out of bounds");
            }
            if (type > maxType) {
                throw std::exception("type out of bounds");
            }
        }

        PersistentBuildState::Key _key;
        uint64_t _id;
        BTree::TreeIndex _type;
    };

    BTree::PersistentPagePool* createPagePool(std::filesystem::path const& path) {
        std::filesystem::create_directory(path.parent_path());
        // Determine stored page size (if any)...
        const BTree::PageSize pageSize = 63*1024;
        BTree::PageSize storedPageSize = BTree::PersistentPagePool::pageCapacity(path.string());
        return new BTree::PersistentPagePool(((0 < storedPageSize) ? storedPageSize : pageSize), path.string());
    }

    BTree::Forest* createForest(
        BTree::PersistentPagePool& pool, 
        std::map<BTree::TreeIndex, BTree::StreamingTree<PersistentBuildState::Key>*>& typeToTree
    ) {
        auto forest = new BTree::Forest(pool);
        for (auto tid : buildStateTypes.ids) {
            auto index = static_cast<BTree::TreeIndex>(tid);
            BTree::StreamingTree<PersistentBuildState::Key>* tree;
            if (forest->contains(index)) {
                tree = forest->accessStreamingTree<PersistentBuildState::Key>(index);
            } else {
                tree = forest->plantStreamingTree<PersistentBuildState::Key>(index);
            }
            typeToTree.insert({ index, tree });
        }
        return forest;
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
        , _pool(createPagePool(directory/"buildstate.bt"))
        , _nextId(1)
    {
        _forest.reset(createForest(*_pool, _typeToTree));
    }

    PersistentBuildState::~PersistentBuildState() {
        _forest.reset();
        _pool.reset();
    }

    void PersistentBuildState::retrieve() {
        reset();
        retrieveAll();
        for (auto pair : _keyToObject) addToBuildState(pair.second);
    }

    void PersistentBuildState::reset() {
        _keyToObject.clear();
        _objectToKey.clear();
        _nextId = 1;
        _toInsert.clear();
        _toReplace.clear();
        _toRemove.clear();
        _toRemoveKeys.clear();
        _context->clearBuildState();
    }

    void PersistentBuildState::retrieveAll() {
        // First instantiate all objects to prevent re-entrant retrieval...
        for (auto pair : _typeToTree) {
            BTree::TreeIndex type = pair.first;
            auto tree = pair.second;
            auto size = tree->size();
            for (auto& reader : *tree) {
                PersistentBuildState::Key key = reader.key();
                KeyCode code(key);
                if (code._type != type) {
                    throw std::exception("type mismatch");
                }
                reader.close();
                if (code._id >= _nextId) _nextId = code._id + 1;
                std::shared_ptr<IPersistable> object(buildStateTypes.instantiate(code._type));
                auto pair = _keyToObject.insert({ key, object });
                _objectToKey.insert({ object.get(), key });
            }
        }        
        // ...then retrieve the objects from the btree.
        // This approach results in retrieval of objects in key order to
        // achieve maximum btree retrieve performance.
        for (auto pair : _typeToTree) {
            uint8_t type = pair.first;
            auto tree = pair.second;
            for (auto& reader : *tree) {
                PersistentBuildState::Key key = reader.key();
                retrieveKey(key, reader);
            }
        }
        for (auto pair : _keyToObject) pair.second->restore(_context);
    }

    void PersistentBuildState::retrieveKey(Key key) {
        KeyCode code(key);
        auto tree = _typeToTree[code._type];
        auto& btreeVReader = tree->retrieve(key);
        retrieveKey(key, btreeVReader);
    }

    void PersistentBuildState::retrieveKey(PersistentBuildState::Key key, BTree::ValueReader<PersistentBuildState::Key>& btreeVReader) {
        YAM::ValueStreamer vReader(btreeVReader);
        SharedPersistableReader snReader(*this);
        Streamer reader(&vReader, &snReader);
        std::shared_ptr<IPersistable> object = _keyToObject[key];
        object.get()->stream(&reader);
        btreeVReader.close();
    }

    std::shared_ptr<IPersistable> PersistentBuildState::retrieve(Key key) {
        return _keyToObject[key];
    }

    std::size_t PersistentBuildState::store() {
        std::unordered_set<std::shared_ptr<IPersistable>> buildState;
        std::unordered_set<std::shared_ptr<IPersistable>> storedState;
        _context->getBuildState(buildState);
        getStoredState(storedState);
        _context->computeStorageNeed(buildState, storedState, _toInsert, _toReplace, _toRemove);

        for (auto const& p : _toRemove) {
           // Save keys of removed object to enable rollback
            Key key = _objectToKey[p.get()];
            _toRemoveKeys.insert({p.get(), key});
            remove(key, p);
            p->modified(false);
        }

        // First bind keys to new objects to avoid re-entrant storage...
        for (auto const& p : _toInsert) bindToKey(p);
        // ...then store the new objects
        for (auto const& p : _toInsert) {
            store(p, false);
            p->modified(false);
        }

        for (auto const& p : _toReplace) {
            store(p, true);
            p->modified(false);
        }

        std::size_t nStored = _toInsert.size() + _toReplace.size() + _toRemove.size();
        try { _forest->commit(); } 
        catch (std::string msg) {
            rollback(true);
            nStored = -1;
        }

        _toInsert.clear();
        _toReplace.clear();
        _toRemove.clear();
        _toRemoveKeys.clear();
        return nStored;
    }

    PersistentBuildState::Key PersistentBuildState::bindToKey(std::shared_ptr<IPersistable> const& object) {
        Key key = allocateKey(object.get());
        _objectToKey.insert({ object.get(), key });
        _keyToObject.insert({ key, object });
        return key;
    }

    PersistentBuildState::Key PersistentBuildState::allocateKey(IPersistable* object) {
        KeyCode code(_nextId, static_cast<uint8_t>(buildStateTypes.getTypeId(object)));
        _nextId += 1;
        return code._key;
    }

    void PersistentBuildState::store(std::shared_ptr<IPersistable> const& object, bool replace) {
        Key key = _objectToKey[object.get()];
        KeyCode code(key);
        auto tree = _typeToTree[code._type];
        auto& btreeVWriter = tree->insert(key);
        YAM::ValueStreamer vWriter(btreeVWriter);
        SharedPersistableWriter snWriter(*this);
        Streamer writer(&vWriter, &snWriter);
        object->stream(&writer);
        btreeVWriter.close();
    }

    PersistentBuildState::Key PersistentBuildState::store(std::shared_ptr<IPersistable> const& object) {
        if (
            object->modified() 
            && !_toInsert.contains(object)
            && !_toReplace.contains(object)
        ) {
            throw std::exception("object not registered for storage");
        }
        return _objectToKey[object.get()];
    }

    void PersistentBuildState::remove(Key key, std::shared_ptr<IPersistable> const& object) {
        _keyToObject.erase(key);
        _objectToKey.erase(object.get());
        KeyCode code(key);
        auto tree = _typeToTree[code._type];
        tree->erase(code._id);
    }

    void PersistentBuildState::rollback(bool recoverForest) {
        if (recoverForest) _forest->recover();
        for (auto const& object : _toRemove) {
            if (recoverForest) {
                Key key = _toRemoveKeys[object.get()];
                _keyToObject.insert({ key, object });
                _objectToKey.insert({ object.get(), key });
            }
            addToBuildState(object);
        }
        for (auto const& object : _toInsert) {
            if (recoverForest) {
                auto it = _objectToKey.find(object.get());
                if (it != _objectToKey.end()) {
                    Key key = it->second;
                    _keyToObject.erase(key);
                    _objectToKey.erase(object.get());
                }
            }
            removeFromBuildState(object);
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

    void PersistentBuildState::addToBuildState(std::shared_ptr<IPersistable> const& object) {
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

    void PersistentBuildState::removeFromBuildState(std::shared_ptr<IPersistable> const& object) {
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
}
