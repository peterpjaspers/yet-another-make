#pragma once

#include "IIOStream.h"
#include <cstddef>
#include <limits>

namespace YAM
{
    // This class supports streaming bytes to/from a contiguously allocated 
    // memory buffer.
    // Not thread-safe.
    //
    // Users of this class must be well-aware of the somewhat odd
    // memory usage behavior of this class, as explained below.
    //
    // The buffer is not used cyclicly, i.e. always m_write >= m_read.
    // This implies that a write may increase buffer capacity when
    // m_read < m_write. Read and write positions will be reset to 0
    // when all readable bytes have been read.
    // For streams with fixed capacity that means that one can write
    // until m_write == capacity. New writes can only be done once
    // all bytes have been read.
    // For streams with non-fixed capicity this means that capacity
    // will be increased when m_write + nBytesToWrite > capacity.
    // This behavior will only stop once all bytes are read.
    //
    // An advantage of this approach is that memory can always be
    // directly accessed linearly via GetRead/WriteBuffer functions.
    //
    class __declspec(dllexport) MemoryStream : public IOutputStream, public IInputStream
    {
    public:
        // Create stream with default initial capacity
        // that will be increased when needed.
        MemoryStream();

        // Create stream with given initial capacity (in bytes)
        // that will be increased when needed.
        // Pre: capacity > 0
        MemoryStream(std::size_t capacity);

        // Create stream given initial capacity (in bytes)
        // and configured with fixed or variable capacity.
        // Variable: will be increased when needed.
        // Fixed: will throw exception when no free space.
        // Pre: capacity > 0
        MemoryStream(std::size_t capacity, bool fixedCapacity);

        virtual ~MemoryStream();

        // Return capacity of memory buffer in bytes.
        std::size_t capacity() const;

        // If newCapacity > Capacity(): increase to newCapacity
        // Throw EndOfStreamException when increase is needed
        // and fixed capacity.
        void grow(std::size_t newCapacity);

        // grow(m_write+nBytes).
        // Write nBytes bytes to stream.
        void write(const void* bytes, std::size_t nBytes) override;

        // Return how many bytes can still be written.
        // Fixed capacity: m_capacity - m_write
        // Non-fixed: UINT_MAX;
        std::size_t writableBytes() const;

        // Return whether nBytes can be written.
        bool canWrite(std::size_t nBytes) const;

        // Grow(m_write+nBytes).
        // Return pointer to a buffer were nBytes can be written.
        void* getWriteBuffer(std::size_t nBytes);

        // Return pointer to a buffer were nBytes can be written.
        // Do NOT increase write position. Call CommitWrite to
        // record the actual written number of bytes.
        void* startWrite(std::size_t nBytes);

        // Increase write position with nBytes.
        void commitWrite(std::size_t nBytes);

        // Read nBytes bytes from stream.
        // Throw EndOfStreamException when reading beyond end-of-stream
        // (i.e. ReadableBytes < nBytes).
        void read(void *bytes, std::size_t nBytes) override;

        // Return number of bytes that can be read from stream, i.e.
        // m_write-m_read.
        std::size_t readableBytes() const;

        // Return whether nBytes bytes can be read from stream.
        bool canRead(std::size_t nBytes) const;

        // Return pointer to a buffer from which nBytes can be read.
        // Increase read position with nBytes.
        void* getReadBuffer(std::size_t nBytes);

        // Return pointer to a buffer were nBytes can be read.
        // Do NOT increase read position. Call CommitRead to
        // record the actual read number of bytes.
        void* startRead(std::size_t nBytes);

        // Increase read position with nBytes
        void commitRead(std::size_t nBytes);

        bool eos() override { return readableBytes() == 0; }
        void flush() override {}

    private:
        MemoryStream(const MemoryStream&);
        MemoryStream& operator=(const MemoryStream&);

        // buffer of m_capacity bytes that can be configured to be of fixed capacity.
        std::size_t m_capacity;
        bool m_fixedCapacity;
        void* m_buffer;
        
        // m_buffer+m_write points to the write position.
        std::size_t m_write;

        // m_buffer+m_read points to the read position.
        // Invariant: m_read <= m_write
        std::size_t m_read;
    };
}