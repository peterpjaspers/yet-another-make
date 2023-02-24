#include "BinaryStreamer.h"

namespace
{
    template<typename Str>
    void writeString(Str& string, YAM::IOutputStream* ioStream) {
        typedef Str::value_type VT;
        uint32_t nChars = static_cast<uint32_t>(string.length());
        uint32_t nBytes = nChars * sizeof(VT);
        ioStream->write(static_cast<void*>(&nBytes), sizeof(nBytes));
        ioStream->write(static_cast<const void*>(string.c_str()), nBytes);
    }

    template<typename Str>
    void readString(Str& string, YAM::IInputStream* ioStream) {
        typedef Str::value_type VT;
        uint32_t nBytes;
        ioStream->read(static_cast<void*>(&nBytes), sizeof(nBytes));
        uint32_t nChars = nBytes / sizeof(VT);
        std::unique_ptr<VT> buf(new VT[nChars]);
        ioStream->read(buf.get(), nBytes);
        string.clear();
        string.insert(0, buf.get(), nChars);
    }
}

namespace YAM
{
    BinaryWriter::BinaryWriter(IStreamableTypes* types, IOutputStream* ioStream)
        : _types(types)
        , _stream(ioStream)
    {}

    BinaryWriter::BinaryWriter(IOutputStream* ioStream)
        : _types(nullptr)
        , _stream(ioStream)
    {}

    void BinaryWriter::stream(void* bytes, unsigned int nBytes) { _stream->write(bytes, nBytes); }
    void BinaryWriter::stream(bool& value) { _stream->write(static_cast<void*>(&value), sizeof(value)); }
    void BinaryWriter::stream(float& value) { _stream->write(static_cast<void*>(&value), sizeof(value)); }
    void BinaryWriter::stream(double& value) { _stream->write(static_cast<void*>(&value), sizeof(value)); }
    void BinaryWriter::stream(int8_t& value) { _stream->write(static_cast<void*>(&value), sizeof(value)); }
    void BinaryWriter::stream(uint8_t& value) { _stream->write(static_cast<void*>(&value), sizeof(value)); }
    void BinaryWriter::stream(int16_t& value) { _stream->write(static_cast<void*>(&value), sizeof(value)); }
    void BinaryWriter::stream(uint16_t& value) { _stream->write(static_cast<void*>(&value), sizeof(value)); }
    void BinaryWriter::stream(int32_t& value) { _stream->write(static_cast<void*>(&value), sizeof(value)); }
    void BinaryWriter::stream(uint32_t& value) { _stream->write(static_cast<void*>(&value), sizeof(value)); }
    void BinaryWriter::stream(int64_t& value) { _stream->write(static_cast<void*>(&value), sizeof(value)); }
    void BinaryWriter::stream(uint64_t& value) { _stream->write(static_cast<void*>(&value), sizeof(value)); }

    void BinaryWriter::stream(std::string& value) { writeString(value, _stream); }
    void BinaryWriter::stream(std::wstring& value) { writeString(value, _stream); }

    void BinaryWriter::stream(IStreamable** streamable) {
        if (_types == nullptr) throw std::exception("Cannot stream type");
        uint32_t objectIndex;
        if (*streamable == nullptr) {
            objectIndex = INT_MAX;
            stream(objectIndex);
        } else if (_objects.find(*streamable) != _objects.end()) {
            objectIndex = _objects[*streamable];
            stream(objectIndex);
        } else {
            objectIndex = static_cast<uint32_t>(_objects.size());
            _objects[*streamable] = objectIndex;
            stream(objectIndex);
            _types->streamType(this, streamable);
            (*streamable)->stream(this);
        }
    }

    void BinaryWriter::stream(std::shared_ptr<IStreamable>& streamable) {
        IStreamable* raw = streamable.get();
        stream(&raw);
    }

    bool BinaryWriter::eos() { return false; }
    void BinaryWriter::flush() { return _stream->flush(); }
}

namespace YAM
{
    BinaryReader::BinaryReader(IStreamableTypes * types, IInputStream * ioStream)
        : _types(types)
        , _stream(ioStream)
    {}

    BinaryReader::BinaryReader(IInputStream * ioStream)
        : _types(nullptr)
        , _stream(ioStream)
    {}

    void BinaryReader::stream(void* bytes, unsigned int nBytes) { _stream->read(bytes, nBytes); }
    void BinaryReader::stream(bool& value) { _stream->read(static_cast<void*>(&value), sizeof(value)); }
    void BinaryReader::stream(float& value) { _stream->read(static_cast<void*>(&value), sizeof(value)); }
    void BinaryReader::stream(double& value) { _stream->read(static_cast<void*>(&value), sizeof(value)); }
    void BinaryReader::stream(int8_t& value) { _stream->read(static_cast<void*>(&value), sizeof(value)); }
    void BinaryReader::stream(uint8_t& value) { _stream->read(static_cast<void*>(&value), sizeof(value)); }
    void BinaryReader::stream(int16_t& value) { _stream->read(static_cast<void*>(&value), sizeof(value)); }
    void BinaryReader::stream(uint16_t& value) { _stream->read(static_cast<void*>(&value), sizeof(value)); }
    void BinaryReader::stream(int32_t& value) { _stream->read(static_cast<void*>(&value), sizeof(value)); }
    void BinaryReader::stream(uint32_t& value) { _stream->read(static_cast<void*>(&value), sizeof(value)); }
    void BinaryReader::stream(int64_t& value) { _stream->read(static_cast<void*>(&value), sizeof(value)); }
    void BinaryReader::stream(uint64_t& value) { _stream->read(static_cast<void*>(&value), sizeof(value)); }

    void BinaryReader::stream(std::string& value) { readString(value, _stream); }
    void BinaryReader::stream(std::wstring& value) { readString(value, _stream); }

    void BinaryReader::stream(IStreamable** streamable) {
        uint32_t objectIndex;
        stream(objectIndex);
        if (objectIndex == INT_MAX) {
            *streamable = NULL;
        } else if (objectIndex == static_cast<uint32_t>(_objects.size())) {
            // first-time reference to this object;
            _types->streamType(this, streamable);
            (*streamable)->stream(this);
            _objects.push_back(*streamable);
        } else if (objectIndex < static_cast<int>(_objects.size())) {
            // N-th reference to object (N > 1)
            *streamable = _objects[objectIndex];
        } else {
            throw std::exception("corrupt stream");
        }
    }

    void BinaryReader::stream(std::shared_ptr<IStreamable>& streamable) {
        IStreamable* raw;
        stream(&raw);
        auto it = _sharedObjects.find(raw);
        if (it == _sharedObjects.end()) {
            streamable = std::shared_ptr<IStreamable>(raw);
            _sharedObjects.insert({ raw, streamable });
        } else {
            streamable = it->second;
        }
    }

    bool BinaryReader::eos() { return _stream->eos(); }
    void BinaryReader::flush() {  }
}
