#pragma once

#include "EndOfStreamException.h"
#include <string>

// In/output streams provide an abstraction to devices that can store/transmit
// data in a sequential manner.
// Examples of such devices are: memory buffer, tcp connection, serial I/O port.

namespace YAM
{
    class __declspec(dllexport) IOutputStream
    {
    public:
        virtual ~IOutputStream() {}

        // Throw EndOfStreamException when writing beyond end-of-stream
        virtual void write(const void* bytes, std::size_t nBytes) = 0;

        // Flush buffered data, if any, to the output device.
        virtual void flush() {}
    };

    // Input streams may buffer available data in memory to more optimally
    // use communication bandwidth.
    class __declspec(dllexport) IInputStream
    {
    public:
        virtual ~IInputStream() {}

        // Throw EndOfStreamException when reading beyond end-of-stream
        virtual void read(void *bytes, std::size_t nBytes) = 0;

        // Return whether no more data can be read.
        virtual bool eos() = 0;
    };
}