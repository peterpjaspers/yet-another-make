#include "ExecutionContext.h"
#include "PersistentBuildState.h"
#include "BuildFileCompilerNode.h"
#include "BuildFileParserNode.h"
#include "CommandNode.h"
#include "ForEachNode.h"
#include "DirectoryNode.h"
#include "DotIgnoreNode.h"
#include "FileExecSpecsNode.h"
#include "GeneratedFileNode.h"
#include "GlobNode.h"
#include "GroupNode.h"
#include "SourceFileNode.h"
#include "RepositoriesNode.h"
#include "FileRepositoryNode.h"
#include "BinaryValueStreamer.h"
#include "ISharedObjectStreamer.h"
#include "Streamer.h"

#include "../btree/Forest.h"
#include "../btree/PersistentPagePool.h"

#include <memory>
#include <filesystem>
#include <set>

namespace
{
    struct ComparePName {
        constexpr bool operator()(const std::shared_ptr<IPersistable>& lhs, const std::shared_ptr<IPersistable>& rhs) const {
            return lhs->describeName() < rhs->describeName();
        }
    };

    void strStream(std::stringstream& ss, std::unordered_set<std::shared_ptr<Node>>const& persistables) {
        if (persistables.empty()) return;
        std::vector<std::shared_ptr<IPersistable>> sorted(persistables.begin(), persistables.end());
        ComparePName cmpName;
        std::sort(sorted.begin(), sorted.end(), cmpName);
        for (auto const& p : sorted) {
            ss << "\t" << p->describeName() << " : " << p->describeType() << std::endl;
        }
    }

    void logDifference(
        ILogBook& logBook,
        std::unordered_set<std::shared_ptr<Node>>const& toInsert,
        std::unordered_set<std::shared_ptr<Node>>const& toReplace,
        std::unordered_set<std::shared_ptr<Node>>const& toRemove,
        bool rollback = false
    ) {
        if (!logBook.mustLogAspect(LogRecord::BuildStateUpdate)) return;

        std::stringstream ss;
        ss << "Persistent buildstate updates:" << std::endl;
        if (!toInsert.empty()) {
            if (rollback) {
                ss << "Rollback insertion of: " << std::endl;
            } else {
                ss << "Insert new objects: " << std::endl;
            }
            strStream(ss, toInsert);
        }
        if (!toReplace.empty()) {
            if (rollback) {
                ss << "Rollback replacement of: " << std::endl;
            } else {
                ss << "Replace objects: " << std::endl;
            }
            strStream(ss, toReplace);
        }
        if (!toRemove.empty()) {
            if (rollback) {
                ss << "Rollback removal of: " << std::endl;
            } else {
                ss << "Remove objects: " << std::endl;
            }
            strStream(ss, toRemove);
        }
        LogRecord r(LogRecord::Aspect::BuildStateUpdate, ss.str());
        logBook.add(r);
    }

    void logPersistentState(
        ILogBook& logBook,
        std::map<PersistentBuildState::Key, std::shared_ptr<IPersistable>> const& keyToObject
    ) {
        if (!logBook.mustLogAspect(LogRecord::BuildStateUpdate)) return;

        std::stringstream ss;
        ss << "Number of objects in persistent buildstate: " << keyToObject.size() << std::endl;
        for (auto const& pair : keyToObject) {
            ss 
                << std::hex << pair.first 
                << " " << pair.second->describeName() 
                << " " << pair.second->describeType()
                << std::endl;
        }
        LogRecord r(LogRecord::Aspect::BuildStateUpdate, ss.str());
        logBook.add(r);
    }
}

namespace YAM
{
    PersistentBuildState::Key nullPtrKey = UINT64_MAX;

    // Class that allocates unique type ids to the node classes. 
    class BuildStateTypes
    {
    public:
        enum TypeId : BTree::TreeIndex {
            Min = 0,
            BuildFileCompilerNode = 1,
            BuildFileParserNode = 2,
            CommandNode = 3,
            DirectoryNode = 4,
            DotIgnoreNode = 5,
            FileExecSpecsNode = 6,
            ForEachNode = 7,
            GeneratedFileNode = 8,
            GlobNode = 9,
            GroupNode = 10,
            RepositoriesNode = 11,
            SourceFileNode = 12,
            FileRepositoryNode = 13,
            Max = 14
        };
        std::vector<TypeId> ids;

        BuildStateTypes() {
            YAM::BuildFileCompilerNode::setStreamableType(static_cast<uint32_t>(BuildFileCompilerNode));
            YAM::BuildFileParserNode::setStreamableType(static_cast<uint32_t>(BuildFileParserNode));
            YAM::CommandNode::setStreamableType(static_cast<uint32_t>(CommandNode));
            YAM::DirectoryNode::setStreamableType(static_cast<uint32_t>(DirectoryNode));
            YAM::DotIgnoreNode::setStreamableType(static_cast<uint32_t>(DotIgnoreNode));
            YAM::FileExecSpecsNode::setStreamableType(static_cast<uint32_t>(FileExecSpecsNode));
            YAM::ForEachNode::setStreamableType(static_cast<uint32_t>(ForEachNode));
            YAM::GeneratedFileNode::setStreamableType(static_cast<uint32_t>(GeneratedFileNode));
            YAM::GlobNode::setStreamableType(static_cast<uint32_t>(GlobNode));
            YAM::GroupNode::setStreamableType(static_cast<uint32_t>(GroupNode));
            YAM::RepositoriesNode::setStreamableType(static_cast<uint32_t>(RepositoriesNode));
            YAM::SourceFileNode::setStreamableType(static_cast<uint32_t>(SourceFileNode));
            YAM::FileRepositoryNode::setStreamableType(static_cast<uint32_t>(FileRepositoryNode));

            ids.push_back(BuildFileCompilerNode);
            ids.push_back(BuildFileParserNode);
            ids.push_back(CommandNode);
            ids.push_back(DirectoryNode);
            ids.push_back(DotIgnoreNode);
            ids.push_back(FileExecSpecsNode);
            ids.push_back(ForEachNode);
            ids.push_back(GeneratedFileNode);
            ids.push_back(GlobNode);
            ids.push_back(GroupNode);
            ids.push_back(RepositoriesNode);
            ids.push_back(SourceFileNode);
            ids.push_back(FileRepositoryNode);
        }

        uint32_t getTypeId(IPersistable* object) {
            auto tid = object->typeId();
            if ((TypeId::Min < tid) && (tid < TypeId::Max)) return tid;
            throw std::exception("unknown node type");
        }

        std::shared_ptr<IPersistable> instantiate(uint32_t typeId) {
            auto tid = static_cast<TypeId>(typeId);
            switch (tid) {
            case TypeId::BuildFileCompilerNode: return std::make_shared<YAM::BuildFileCompilerNode>();
            case TypeId::BuildFileParserNode: return std::make_shared<YAM::BuildFileParserNode>();
            case TypeId::CommandNode: return std::make_shared<YAM::CommandNode>();
            case TypeId::DirectoryNode: return std::make_shared<YAM::DirectoryNode>();
            case TypeId::DotIgnoreNode: return std::make_shared<YAM::DotIgnoreNode>();
            case TypeId::FileExecSpecsNode: return std::make_shared<YAM::FileExecSpecsNode>();
            case TypeId::ForEachNode: return std::make_shared<YAM::ForEachNode>();
            case TypeId::GeneratedFileNode: return std::make_shared<YAM::GeneratedFileNode>();
            case TypeId::GlobNode: return std::make_shared<YAM::GlobNode>();
            case TypeId::GroupNode: return std::make_shared<YAM::GroupNode>();
            case TypeId::RepositoriesNode: return std::make_shared<YAM::RepositoriesNode>();
            case TypeId::SourceFileNode: return std::make_shared<YAM::SourceFileNode>();
            case TypeId::FileRepositoryNode: return std::make_shared<YAM::FileRepositoryNode>();
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
        ValueStreamer(BTree::ValueWriter<PersistentBuildState::Key>& streamer)
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
            if (object == nullptr) {
                writer->stream(nullPtrKey);
            } else {
                auto persistable = dynamic_pointer_cast<IPersistable>(object);
                PersistentBuildState::Key key = _buildState.getKey(persistable);
                writer->stream(key);
            }
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
            if (key == nullPtrKey) {
                object.reset();
            } else {
                object = _buildState.getObject(key);
            }
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
            , _id(key& idMask)
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
        // Determine stored page size (if any)...
        const BTree::PageSize pageSize = 32 * 1024;
        BTree::PageSize storedPageSize = BTree::PersistentPagePool::pageCapacity(path.string());
        return new BTree::PersistentPagePool(((0 < storedPageSize) ? storedPageSize : pageSize), path.string());
    }

    std::shared_ptr<BTree::Forest> createForest(
        BTree::PersistentPagePool& pool,
        std::map<BTree::TreeIndex, BTree::StreamingTree<PersistentBuildState::Key>*>& typeToTree
    ) {
        auto forest = std::make_shared<BTree::Forest>(pool);
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
        std::filesystem::path const& stateFile,
        ExecutionContext* context
    )
        : _stateFile(stateFile)
        , _context(context)
        , _pool(createPagePool(stateFile))
        , _forest(createForest(*_pool, _typeToTree))
        , _nextId(1)
    {
    }

    PersistentBuildState::~PersistentBuildState() {
        _forest.reset();
        _pool.reset();
    }

    bool PersistentBuildState::isPendingDelete(std::string const& name) const
    {
        for (auto const& pair : _deletedObjectToKey) {
            if (pair.first->describeName() == name) return true;
        }
        return false;
    }

    void PersistentBuildState::logState(ILogBook& logBook) {
        logPersistentState(logBook, _keyToObject);
    }

    void PersistentBuildState::retrieve() {
        reset();
        retrieveAll();
        for (auto const& pair : _keyToObject) {
            auto key = pair.first;
            auto const& object = pair.second;
            if (object->deleted()) {
                _keyToDeletedObject.insert({ key, object });
                _deletedObjectToKey.insert({ object.get(), key });
            } else {
                addToBuildState(pair.second);
            }
        }
        std::vector<Key> garbageKeys;
        for (auto const& pair : _keyToDeletedObject) {
            auto &key = pair.first;
            auto &object = pair.second;
            _keyToObject.erase(key);
            _objectToKey.erase(object.get());
            if (object.use_count() == 1) {
                garbageKeys.push_back(key);
            }
        }
        if (!garbageKeys.empty()) {
            for (int i = 0; i < garbageKeys.size(); ++i) {
                auto &key = garbageKeys[i];
                if (!removePendingDelete(key)) {
                    throw std::runtime_error("Failed to remove object from storage");
                }
            }
            _forest->commit();
        }
        std::unordered_set<IPersistable const*> restored;
        for (auto const& pair : _keyToObject) pair.second->restore(_context, restored);
        for (auto const& pair : _keyToDeletedObject) pair.second->restore(_context, restored);
        _context->nodes().clearChangeSet();
    }

    void PersistentBuildState::reset() {
        _keyToObject.clear();
        _objectToKey.clear();
        _keyToDeletedObject.clear();
        _deletedObjectToKey.clear();
        _nextId = 1;
        _context->clearBuildState();
    }

    void PersistentBuildState::retrieveAll() {
        // First instantiate all objects to prevent re-entrant retrieval...
        for (auto pair : _typeToTree) {
            BTree::TreeIndex type = pair.first;
            auto tree = pair.second;
            for (auto& reader : *tree) {
                PersistentBuildState::Key key = reader.key();
                KeyCode code(key);
                if (code._type != type) {
                    throw std::exception("type mismatch");
                }
                reader.close();
                if (code._id >= _nextId) _nextId = code._id + 1;
                std::shared_ptr<IPersistable> object(buildStateTypes.instantiate(code._type));
                _keyToObject.insert({ key, object });
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
    }

    void PersistentBuildState::retrieveKey(Key key) {
        KeyCode code(key);
        auto tree = _typeToTree[code._type];
        auto& btreeVReader = tree->at(key);
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

    // Called by SharedPersistableReader to get the object stored at key
    std::shared_ptr<IPersistable> PersistentBuildState::getObject(Key key) {
        std::shared_ptr<IPersistable> object; 
        auto dit = _keyToDeletedObject.find(key);
        if (dit != _keyToDeletedObject.end()) {
            object = dit->second;
        } else {
            auto it = _keyToObject.find(key); 
            if (it == _keyToObject.end()) {
                throw std::runtime_error("Attempt to getObject with unknown key");
            }
            object = it->second;
        }
        return object;
    }

    std::size_t PersistentBuildState::store() {
        if (_context->nodes().changeSetSize() == 0) return 0;
        std::unordered_set<std::shared_ptr<IPersistable>> toReplaceDeleted;
        auto& nodes = _context->nodes();
        auto const &toInsert = nodes.addedNodes();
        auto const &toReplace = nodes.modifiedNodes();
        auto const &toRemove = nodes.removedNodes();
        logDifference(*(_context->logBook()), toInsert, toReplace, toRemove);

        for (auto const& p : toRemove) {
            if (!p->deleted()) {
                throw std::exception("Wrong state to delete");
            }
            auto pit = _deletedObjectToKey.find(p.get());
            if (pit != _deletedObjectToKey.end()) {
                throw std::exception("Attempt to delete a pending delete object.");
            }
            auto it = _objectToKey.find(p.get());
            if (it == _objectToKey.end()) {
                // Happens when node was added and again removed since previous
                // store() call.
            } else {
                Key key = it->second;
                // 2 strong refs left to p: from _toRemove and from _keyToObject,
                // i.e. p is not referenced by other objects and can be safely
                // removed from storage.
                if (p.use_count() == 2) {
                    if (!remove(key, p)) {
                        throw std::runtime_error("Failed to delete object from storage");
                    }
                } else {
                    // postpone deletion from forest until p is no longer referenced
                    _keyToObject.erase(key);
                    _objectToKey.erase(p.get());
                    _keyToDeletedObject.insert({ key, p });
                    _deletedObjectToKey.insert({ p.get(), key });
                    toReplaceDeleted.insert(p);
                }
            }
        }

        // First bind keys to new objects to avoid re-entrant storage...
        for (auto const& p : toInsert) bindToKey(p);
        // ...then store the new objects
        for (auto const& p : toInsert) {
            store(_objectToKey[p.get()], p);
            p->modified(false);
        }

        for (auto const& p : toReplace) {
            store(_objectToKey[p.get()], p);
            p->modified(false);
        }
        for (auto const& p : toReplaceDeleted) {
            store(_deletedObjectToKey[p.get()], p);
            p->modified(false);
        }

        std::size_t nStored = 
            toInsert.size()
            + toReplace.size()
            + toRemove.size()
            + toReplaceDeleted.size();
        nodes.clearChangeSet();
        if (0 < nStored) {
            try { _forest->commit(); } 
            catch (std::string msg) {
                // This should be a very rare error. Therefore no attempt
                // is made to optimize recovery.
                _forest->recover();
                retrieve();
                nStored = -1;
            }
        }
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

    void PersistentBuildState::store(Key key, std::shared_ptr<IPersistable> const& object) {
        //std::stringstream ss;
        //ss << "Insert  " << std::hex << key;
        //LogRecord r(LogRecord::Aspect::Progress, ss.str());
        //_context->addToLogBook(r);

        KeyCode code(key);
        auto tree = _typeToTree[code._type];
        auto& btreeVWriter = tree->insert(key);
        YAM::ValueStreamer vWriter(btreeVWriter);
        SharedPersistableWriter snWriter(*this);
        Streamer writer(&vWriter, &snWriter);
        object->stream(&writer);
        btreeVWriter.close();
    }

    // Called by SharedPersistableWriter to get the key of a stored object.
    PersistentBuildState::Key PersistentBuildState::getKey(std::shared_ptr<IPersistable> const& object) {
        Key key;

        if (object->deleted()) {
            auto dit = _deletedObjectToKey.find(object.get());
            if (dit == _deletedObjectToKey.end()) {
                throw std::exception("unknown object");
            }
            key = dit->second;
        } else {
            auto it = _objectToKey.find(object.get());
            if (it == _objectToKey.end()) {
                throw std::exception("unknown object");
            }
            key = it->second;
        }
        return key;
    }

    bool PersistentBuildState::remove(Key key, std::shared_ptr<IPersistable> const& object) {
        if (!object->deleted()) {
            throw std::exception("cannot remove an object in state !deleted()");
        }
        auto it = _keyToObject.find(key);
        if (it == _keyToObject.end()) {
            throw std::exception("Unknown object");
        }
        _keyToObject.erase(it);
        _objectToKey.erase(object.get());

        //std::stringstream ss;
        //ss << "Erase  " << std::hex << key;
        //LogRecord r(LogRecord::Aspect::Progress, ss.str());
        //_context->addToLogBook(r);

        KeyCode code(key);
        auto tree = _typeToTree[code._type];
        return tree->erase(key);
    }

    bool PersistentBuildState::removePendingDelete(Key key) {
        auto it = _keyToDeletedObject.find(key);
        if (it == _keyToDeletedObject.end()) {
            throw std::exception("Unknown key");
        }
        _deletedObjectToKey.erase(it->second.get());
        _keyToDeletedObject.erase(it);

        //std::stringstream ss;
        //ss << "Erase  " << std::hex << key;
        //LogRecord r(LogRecord::Aspect::Progress, ss.str());
        //_context->addToLogBook(r);

        KeyCode code(key);
        auto tree = _typeToTree[code._type];
        return tree->erase(key);
    }

    void PersistentBuildState::rollback() {
        auto& nodes = _context->nodes();
        std::unordered_set<std::shared_ptr<Node>> toRemove = nodes.addedNodes();
        std::unordered_set<std::shared_ptr<Node>> toReplace = nodes.modifiedNodes();
        std::unordered_set<std::shared_ptr<Node>> toAdd = nodes.removedNodes();

        for (auto const& object : toRemove) {
            removeFromBuildState(object);
        }
        for (auto const& object : toReplace) {
            // Replaces object-in-place by re-streaming it from btree.
            Key key = _objectToKey[object.get()];
            object->prepareDeserialize();
            retrieveKey(key);
        };
        for (auto const& object : toAdd) {
            // toAdd contains the objects that were removed from context
            // since previous storage. On removal from context these objects
            // are cleaned-up, see Node::cleanup(). These objects must therefore
            // be retrieved from storage to restore their state to before
            // cleanup.
            Key key = _objectToKey[object.get()];
            object->prepareDeserialize();
            retrieveKey(key);
            addToBuildState(object);
        }

        if (!toReplace.empty()) {
            std::unordered_set<IPersistable const*> restored;
            for (auto const& pair : _objectToKey) {
                restored.insert(pair.first);
            }
            for (auto const& object : toReplace) {
                restored.erase(object.get());
            }
            for (auto const& object : toAdd) {
                restored.erase(object.get());
            }
            for (auto const& object : toReplace) {
                object->restore(_context, restored);
            }
            for (auto const& object : toAdd) {
                object->restore(_context, restored);
            }
        }
        nodes.clearChangeSet();
    }

    void PersistentBuildState::getStoredState(std::unordered_set<std::shared_ptr<IPersistable>>& storedState) {
        for (auto const& p : _keyToObject) storedState.insert(p.second);
    }

    void PersistentBuildState::addToBuildState(std::shared_ptr<IPersistable> const& object) {
        if (object == nullptr) {
        } else {
            auto node = dynamic_pointer_cast<Node>(object);
            if (node == nullptr) {
                throw std::exception("unknown object class");
            } else {
                _context->nodes().add(node);
                auto repos = dynamic_pointer_cast<RepositoriesNode>(node);
                if (repos != nullptr) _context->repositoriesNode(repos);
            }
        }
    }

    void PersistentBuildState::removeFromBuildState(std::shared_ptr<IPersistable> const& object) {
        auto node = dynamic_pointer_cast<Node>(object);
        if (node != nullptr) {
            _context->nodes().remove(node);
            auto repos = dynamic_pointer_cast<RepositoriesNode>(object);
            if (repos != nullptr) {
                _context->repositoriesNode(nullptr);
            }
        } else {
            throw std::exception("unknown object class");
        }
    }
}
