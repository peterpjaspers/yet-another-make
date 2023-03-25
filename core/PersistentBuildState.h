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
    class __declspec(dllexport) PersistentBuildState
    {
    public:
        typedef uint64_t Key;

        // Construct for storage of build state in given directory.
        PersistentBuildState(
            std::filesystem::path const& directory,
            ExecutionContext* context);

        // Retrieve the build state from storage and replace build state in
        // the execution context with the retrieved build state.
        void retrieve();

        // Store the modified parts of the build state.
        void store();

        // Return the number of objects stored since last retrieve().
        // An object is stored only if it was modified or if it was
        // not yet persisted.
        uint64_t nStored() const;

        // Retrieve the object identified by key. For use in deserialization 
        // of persistable object.
        std::shared_ptr<IPersistable> retrieve(Key key);

        // If object not yet in storage: add it to storage
        // else if object->modified(): update stored object.
        // Return the key of stored object.
        // For use in serialization of persistable object.
        Key insert(std::shared_ptr<IPersistable> const& object);

        // If object is in storage: remove it from storage.
        void remove(std::shared_ptr<IPersistable> const& object);

    private:
        void abort();
        void retrieveAll();
        void commit();

        Key allocateKey(IPersistable* object);

        void retrieveKey(Key key);
        void processRetrieveQueue();

        void insertKey(Key key);
        void processInsertQueue();

        std::filesystem::path _directory;
        ExecutionContext* _context;
        std::shared_ptr<Btree::StreamingTree> _btree;
        std::map<uint8_t, std::shared_ptr<Btree::StreamingTree>> _typeToTree;
        uint64_t _nextId;
        std::map<Key, std::shared_ptr<IPersistable>> _keyToObject;
        std::map<IPersistable*, Key> _objectToKey;
        uint32_t _retrieveNesting;
        uint32_t _insertNesting;
        std::queue<Key> _retrieveQueue;
        std::queue<Key> _insertQueue;
        uint64_t _nStored;
    };

}