#include "MemoryStream.h"

namespace YAM
{
    MemoryStream::MemoryStream()
        : m_capacity(16)
        , m_fixedCapacity(false)
        , m_buffer(malloc(m_capacity))
        , m_write(0)
        , m_read(0)
    {}

    MemoryStream::MemoryStream(std::size_t capacity)
        : m_capacity(capacity)
        , m_fixedCapacity(false)
        , m_buffer(malloc(m_capacity))
        , m_write(0)
        , m_read(0)
    {}

    MemoryStream::MemoryStream(std::size_t capacity, bool fixedCapacity)
        : m_capacity(capacity)
        , m_fixedCapacity(fixedCapacity)
        , m_buffer(malloc(m_capacity))
        , m_write(0)
        , m_read(0)
    {}

    MemoryStream::~MemoryStream()
    {
        if (m_buffer != NULL) free(m_buffer);
    }

    std::size_t MemoryStream::capacity() const
    {
        return m_capacity;
    }

    void MemoryStream::grow(std::size_t newCapacity)
    {
        if (newCapacity > m_capacity) {
            if (m_fixedCapacity) {
                throw EndOfStreamException("Cannot grow buffer capacity");
            }
            m_buffer = realloc(m_buffer, newCapacity);
            m_capacity = newCapacity;
        }
    }

    void MemoryStream::write(const void* bytes, std::size_t nBytes)
    {
        void* dst = getWriteBuffer(nBytes);
        memcpy(dst, bytes, nBytes);
    }

    std::size_t MemoryStream::writableBytes() const
    {
        if (m_fixedCapacity) return m_capacity - m_write;
        return UINT_MAX;
    }

    bool MemoryStream::canWrite(std::size_t nBytes) const
    {
        return writableBytes() >= nBytes;
    }

    void* MemoryStream::getWriteBuffer(std::size_t nBytes)
    {
        void* dst = startWrite(nBytes);
        commitWrite(nBytes);
        return dst;
    }

    void* MemoryStream::startWrite(std::size_t nBytes)
    {
        grow(m_write + nBytes);
        char* dst = static_cast<char*>(m_buffer) + m_write;
        return dst;
    }

    void MemoryStream::commitWrite(std::size_t nBytes)
    {
        if (m_write + nBytes > m_capacity) {
            throw EndOfStreamException("CommitWrite size too large");
        }
        m_write += nBytes;
    }

    void MemoryStream::read(void *bytes, std::size_t nBytes)
    {
        void* src = getReadBuffer(nBytes);
        memcpy(bytes, src, nBytes);
    }

    std::size_t MemoryStream::readableBytes() const
    {
        return m_write - m_read;
    }

    bool MemoryStream::canRead(std::size_t nBytes) const
    {
        return readableBytes() >= nBytes;
    }

    void* MemoryStream::getReadBuffer(std::size_t nBytes)
    {
        void* src = startRead(nBytes);
        commitRead(nBytes);
        return src;
    }

    void* MemoryStream::startRead(std::size_t nBytes)
    {
        const bool hasData = canRead(nBytes);
        if (!hasData) throw EndOfStreamException("Insufficient readable data");
        char* src = static_cast<char*>(m_buffer) + m_read;
        return src;
    }

    void MemoryStream::commitRead(std::size_t nBytes)
    {
        const bool hasData = canRead(nBytes);
        if (!hasData) throw EndOfStreamException("CommitRead size too large");
        m_read += nBytes;
        if (m_write == m_read) {
            m_write = 0;
            m_read = 0;
        }
    }
}
