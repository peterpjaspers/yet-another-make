#pragma once

#include "BinaryValueStreamer.h"
#include "IIOStream.h"

#include <fstream>
#include <memory>
#include <filesystem>
#include <map>
#include <unordered_set>
#include <queue>

namespace
{
    class FileIStream : public YAM::IInputStream
    {
    public:
        FileIStream(std::filesystem::path const& path)
            : _path(path)
            , _file(path.string(), std::ifstream::ate | std::ifstream::binary)
            , _size(_file.tellg())
            , _pos(0)
        {
            _file.seekg(0);
        }

        void read(void* bytes, std::size_t nBytes) override {
            if ((_pos + nBytes) > _size) {
                throw std::exception("eof");
            }
            _pos += nBytes;
            _file.read(reinterpret_cast<char*>(bytes), nBytes);
            if (_file.fail()) {
                auto cnt = _file.gcount();
                bool stop = true;
                throw std::exception("fail");
            }
        }
        ~FileIStream() { _file.close(); }

        bool eos() override { return _file.eof(); }

    private:
        std::filesystem::path _path;
        std::ifstream _file;
        std::size_t _size;
        std::size_t _pos;
    };

    class FileOStream : public YAM::IOutputStream
    {
    public:
        FileOStream(std::filesystem::path const& path)
            : _path(path)
            , _file(path.string(), std::ifstream::binary)
        {}
        void write(const void* bytes, std::size_t nBytes) override {
            _file.write(reinterpret_cast<const char*>(bytes), nBytes);
        }
        ~FileOStream() { close(); }
        void close() override {
            _file.close();
        }

    private:
        std::filesystem::path _path;
        std::ofstream _file;
    };
}

namespace Btree {
    class StreamingTree
    {
    public:
        StreamingTree(std::filesystem::path const& directory) 
        : _directory(directory)
        {
            std::filesystem::create_directories(directory);
        }
        std::vector<uint64_t> keys() { 
            std::vector<uint64_t> keys;
            for (auto entry : std::filesystem::directory_iterator(_directory)) {
                uint64_t key;
                std::stringstream ss(entry.path().filename().string());
                ss >> key;
                keys.push_back(key);
            }
            return keys;
        }

        YAM::IValueStreamer* retrieve(uint64_t key) {
            std::stringstream ss;
            ss << key;
            _fileIStream = std::make_shared<FileIStream>(_directory / ss.str());
            _reader = std::make_shared<YAM::BinaryValueReader>(_fileIStream.get());
            return _reader.get();
        }

        YAM::IValueStreamer* insert(uint64_t key) {
            std::stringstream ss;
            ss << key;
            _fileOStream = std::make_shared<FileOStream>(_directory / ss.str());
            _writer = std::make_shared<YAM::BinaryValueWriter>(_fileOStream.get());
            return _writer.get();
        }

        YAM::IValueStreamer* replace(uint64_t key) {
            return insert(key);
        }

        void remove(uint64_t key) {
            std::stringstream ss;
            ss << key;
            std::filesystem::remove(ss.str());
        }

        void commit() {
            _writer = nullptr;
            _fileOStream = nullptr;
            _reader = nullptr;
            _fileIStream = nullptr;
        }

    private:
        std::filesystem::path _directory;
        std::shared_ptr<YAM::IInputStream> _fileIStream;
        std::shared_ptr<YAM::IValueStreamer> _reader;
        std::shared_ptr<YAM::IOutputStream> _fileOStream;
        std::shared_ptr<YAM::IValueStreamer> _writer;
    };
}

namespace YAM
{
    class ExecutionContext;
    class IPersistable;

    // The YAM build state consists of the nodes and repositories in an
    // ExecutionContext. 
    // Happy flow usage:
    //      PersistentBuildState pstate(directory, context);
    //      pstate.retrieve();
    //      // Add/modify/remove nodes and/or repos in context.
    //      pstate.store();
    // 
    // Non-happy flow usage:
    //      PersistentBuildState pstate(directory, context);
    //      pstate.retrieve();
    //      // Add/modify/remove nodes and/or repos in context.
    //      // Discover corruption of the build state, e.g. changes in a 
    //      // build file introduced a cycle in the node graph.      
    //      pstate.rollback();
    //
    class __declspec(dllexport) PersistentBuildState
    {
    public:
        friend class SharedPersistableReader;
        friend class SharedPersistableWriter;

        typedef uint64_t Key;

        // Construct for storage of build state in given directory.
        PersistentBuildState(
            std::filesystem::path const& directory,
            ExecutionContext* context);

        // Retrieve the build state.
        // Time complexity: O(N) where N is the number of objects in the build
        // state.
        // Post: context contains the retrieved build state.
        void retrieve();

        // Store the build state changes applied since last commit().
        // Time complexity: O(N) where N is the number of new/modified/removed
        // objects in the build state since previous store().
        // Return N.
        // Post:
        //     store() != -1 : commit succeeded.
        //     store() == -1 : commit failed. The build state has been rolled 
        //                     back to the state at last successfull commit.
        std::size_t store();

        // Rollback the build state to its state at last successfull commit.
        void rollback();

    private:
        // retrieve for use by SharedPersistableReader
        std::shared_ptr<IPersistable> retrieve(Key key);
        // store for use by SharedPersistableWriter
        Key store(std::shared_ptr<IPersistable> const& object);

        void reset();
        void retrieveAll();
        void retrieveKey(Key key);

        Key bindToKey(std::shared_ptr<IPersistable> const& object);
        Key allocateKey(IPersistable* object);
        void store(std::shared_ptr<IPersistable> const& object, bool replace);
        bool commitBtrees();

        void remove(Key key, std::shared_ptr<IPersistable> const& object);

        // If recoverBtrees: recover btrees to last commit
        // Restore build state to state at last commit.
        void rollback(bool recoverBtrees); 
        
        // Return in storedState the objects that have a key.
        void getStoredState(std::unordered_set<std::shared_ptr<IPersistable>>& storedState);
        void addToBuildState(std::shared_ptr<IPersistable> const& object);
        void removeFromBuildState(std::shared_ptr<IPersistable> const& object);

        std::filesystem::path _directory;
        ExecutionContext* _context;
        std::shared_ptr<Btree::StreamingTree> _btree;
        std::map<uint8_t, std::shared_ptr<Btree::StreamingTree>> _typeToTree;
        uint64_t _nextId;
        std::map<Key, std::shared_ptr<IPersistable>> _keyToObject;
        std::map<IPersistable*, Key> _objectToKey;
        std::unordered_set<std::shared_ptr<IPersistable>> _toInsert;
        std::unordered_set<std::shared_ptr<IPersistable>> _toReplace;
        std::unordered_set<std::shared_ptr<IPersistable>> _toRemove;
        std::map<IPersistable*, Key> _toRemoveKeys;
    };

}