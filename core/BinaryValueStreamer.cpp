#include "BinaryValueStreamer.h"
#include "IIOStream.h"
#include "IStreamable.h"

namespace YAM
{
    BinaryValueWriter::BinaryValueWriter(IOutputStream* ioStream)
        : _stream(ioStream)
    {}

    void BinaryValueWriter::stream(void* bytes, unsigned int nBytes) { _stream->write(bytes, nBytes); }
    void BinaryValueWriter::stream(bool& value) { _stream->write(static_cast<void*>(&value), sizeof(value)); }
    void BinaryValueWriter::stream(float& value) { _stream->write(static_cast<void*>(&value), sizeof(value)); }
    void BinaryValueWriter::stream(double& value) { _stream->write(static_cast<void*>(&value), sizeof(value)); }
    void BinaryValueWriter::stream(int8_t& value) { _stream->write(static_cast<void*>(&value), sizeof(value)); }
    void BinaryValueWriter::stream(uint8_t& value) { _stream->write(static_cast<void*>(&value), sizeof(value)); }
    void BinaryValueWriter::stream(int16_t& value) { _stream->write(static_cast<void*>(&value), sizeof(value)); }
    void BinaryValueWriter::stream(uint16_t& value) { _stream->write(static_cast<void*>(&value), sizeof(value)); }
    void BinaryValueWriter::stream(int32_t& value) { _stream->write(static_cast<void*>(&value), sizeof(value)); }
    void BinaryValueWriter::stream(uint32_t& value) { _stream->write(static_cast<void*>(&value), sizeof(value)); }
    void BinaryValueWriter::stream(int64_t& value) { _stream->write(static_cast<void*>(&value), sizeof(value)); }
    void BinaryValueWriter::stream(uint64_t& value) { _stream->write(static_cast<void*>(&value), sizeof(value)); }

    void BinaryValueWriter::close() { _stream->close(); }
}

namespace YAM
{
    BinaryValueReader::BinaryValueReader(IInputStream* ioStream)
        : _stream(ioStream)
    {}

    void BinaryValueReader::stream(void* bytes, unsigned int nBytes) { _stream->read(bytes, nBytes); }
    void BinaryValueReader::stream(bool& value) { _stream->read(static_cast<void*>(&value), sizeof(value)); }
    void BinaryValueReader::stream(float& value) { _stream->read(static_cast<void*>(&value), sizeof(value)); }
    void BinaryValueReader::stream(double& value) { _stream->read(static_cast<void*>(&value), sizeof(value)); }
    void BinaryValueReader::stream(int8_t& value) { _stream->read(static_cast<void*>(&value), sizeof(value)); }
    void BinaryValueReader::stream(uint8_t& value) { _stream->read(static_cast<void*>(&value), sizeof(value)); }
    void BinaryValueReader::stream(int16_t& value) { _stream->read(static_cast<void*>(&value), sizeof(value)); }
    void BinaryValueReader::stream(uint16_t& value) { _stream->read(static_cast<void*>(&value), sizeof(value)); }
    void BinaryValueReader::stream(int32_t& value) { _stream->read(static_cast<void*>(&value), sizeof(value)); }
    void BinaryValueReader::stream(uint32_t& value) { _stream->read(static_cast<void*>(&value), sizeof(value)); }
    void BinaryValueReader::stream(int64_t& value) { _stream->read(static_cast<void*>(&value), sizeof(value)); }
    void BinaryValueReader::stream(uint64_t& value) { _stream->read(static_cast<void*>(&value), sizeof(value)); }
}

