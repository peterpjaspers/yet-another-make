#pragma once

#include <string>

// In/output streams provide an abstraction to devices that can store/transmit
// data in a sequential manner.
// Examples of such devices are: memory buffer, tcp connection, serial I/O port.

namespace YAM
{
    struct __declspec(dllexport) EndOfStreamException
    {
    public:
        std::string _message;

        EndOfStreamException(const std::string& message) : _message(message) {}
    };
}
