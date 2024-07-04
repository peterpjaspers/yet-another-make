#include "Streamer.h"
#include "IValueStreamer.h"
#include "ISharedObjectStreamer.h"

namespace YAM
{
    Streamer::Streamer(IValueStreamer* valueStreamer, ISharedObjectStreamer* objectStreamer)
        : _valueStreamer(valueStreamer)
        , _objectStreamer(objectStreamer)
    {}

    bool Streamer::writing() const { return _valueStreamer->writing(); }
    void Streamer::stream(void* bytes, unsigned int nBytes) { _valueStreamer->stream(bytes, nBytes); }
    void Streamer::stream(bool& value) { _valueStreamer->stream(value); }
    void Streamer::stream(float& value) { _valueStreamer->stream(value); }
    void Streamer::stream(double& value) { _valueStreamer->stream(value); }
    void Streamer::stream(int8_t& value) { _valueStreamer->stream(value); }
    void Streamer::stream(uint8_t& value) { _valueStreamer->stream(value); }
    void Streamer::stream(int16_t& value) { _valueStreamer->stream(value); }
    void Streamer::stream(uint16_t& value) { _valueStreamer->stream(value); }
    void Streamer::stream(int32_t& value) { _valueStreamer->stream(value); }
    void Streamer::stream(uint32_t& value) { _valueStreamer->stream(value); }
    void Streamer::stream(int64_t& value) { _valueStreamer->stream(value); }
    void Streamer::stream(uint64_t& value) { _valueStreamer->stream(value); }
    void Streamer::stream(std::shared_ptr<IStreamable>& streamable) {
        _objectStreamer->stream(this, streamable);
    }
}