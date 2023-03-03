#include "SharedObjectStreamer.h"
#include "IObjectStreamer.h"
#include "IStreamer.h"

namespace YAM
{

    SharedObjectWriter::SharedObjectWriter(IObjectStreamer* writer)
        : _owriter(writer)
    {}

    void SharedObjectWriter::stream(IStreamer* writer, std::shared_ptr<IStreamable>& object) {
        IStreamable* raw = object.get();
        uint32_t objectIndex;
        if (raw == nullptr) {
            objectIndex = INT_MAX;
            writer->stream(objectIndex);
        } else if (_objects.find(raw) != _objects.end()) {
            objectIndex = _objects[raw];
            writer->stream(objectIndex);
        } else {
            objectIndex = static_cast<int>(_objects.size());
            _objects[raw] = objectIndex;
            writer->stream(objectIndex);
            _owriter->stream(writer, &raw);
        }
    }
}

namespace YAM
{
    SharedObjectReader::SharedObjectReader(IObjectStreamer* reader)
        : _oreader(reader)
    {}

    void SharedObjectReader::stream(IStreamer* reader, std::shared_ptr<IStreamable>& object) {
        uint32_t objectIndex;
        reader->stream(objectIndex);
        if (objectIndex == INT_MAX) {
            object.reset();
        } else if (objectIndex == static_cast<uint32_t>(_objects.size())) {
            // first-time reference to this object;
            IStreamable* raw;
            _oreader->stream(reader, &raw);
            object.reset(raw);
            _objects.push_back(object);
        } else if (objectIndex < static_cast<int>(_objects.size())) {
            // N-th reference to object (N > 1)
            object = _objects[objectIndex];
        } else {
            throw std::exception("corrupt stream");
        }
    }
}
