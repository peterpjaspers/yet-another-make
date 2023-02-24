#pragma once

namespace YAM
{ 
    class IStreamable;
    class IStreamer;

    // Base for classes that are capable of streaming type information
    // for IStreamable objects and of instantiating (when deserializing)
    // an instance of the streamed type.
    class __declspec(dllexport) IStreamableTypes
    {
    public:
        IStreamableTypes() {};
        virtual ~IStreamableTypes() {};

        // streamer->IsSerializing(): stream type of *streamable to streamer.
        // !streamer->IsSerializing(): stream type from streamer and set
        // *streamable to point to a new instance of the streamed type.
        // Throw NoSuchStreamableTypeException when streamable type is unknown.
        // Serializing a nullptr is valid (i.e. *streamable == nullptr).
        //
        virtual void streamType(IStreamer* streamer, IStreamable** streamable) const = 0;
    };
}