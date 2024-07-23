#include "../MemoryStream.h"
#include <gtest/gtest.h>

namespace
{
    using namespace YAM;

    TEST(MemoryStream, EmptyStream)
    {
        const std::size_t blockSize = 2;
        char readBlock[blockSize];

        MemoryStream stream(blockSize);

        EXPECT_EQ(UINT_MAX, stream.writableBytes());
        EXPECT_TRUE(stream.canWrite(blockSize));
        EXPECT_TRUE(stream.canWrite(0));
        EXPECT_EQ(blockSize, stream.capacity());
        EXPECT_TRUE(stream.canWrite(1000 * blockSize));

        EXPECT_EQ(0, stream.readableBytes());
        EXPECT_FALSE(stream.canRead(1));
        EXPECT_TRUE(stream.canRead(0));
        EXPECT_THROW(stream.read(readBlock, blockSize), EndOfStreamException);
    }

    TEST(MemoryStream, grow)
    {
        const std::size_t blockSize = 2;
        MemoryStream stream(blockSize);
        EXPECT_EQ(blockSize, stream.capacity());
        stream.grow(2 * blockSize);
        EXPECT_EQ(2 * blockSize, stream.capacity());
    }

    TEST(MemoryStream, WriteGrowRead)
    {
        const std::size_t blockSize = 33;
        char writeBlock[blockSize];
        char readBlock[blockSize];

        MemoryStream stream(blockSize);
        for (std::size_t j = 0; j < blockSize; j++) writeBlock[j] = static_cast<char>(j);
        EXPECT_NO_THROW(stream.write(writeBlock, blockSize));
        EXPECT_NO_THROW(stream.write(writeBlock, blockSize));
        EXPECT_EQ(2 * blockSize, stream.capacity());
        EXPECT_EQ(2 * blockSize, stream.readableBytes());
        EXPECT_NO_THROW(stream.canRead(2 * blockSize));
        for (std::size_t i = 0; i < 2; i++)
        {
            EXPECT_NO_THROW(stream.read(readBlock, blockSize));
            for (std::size_t j = 0; j < blockSize; j++)
            {
                EXPECT_EQ(static_cast<char>(j), readBlock[j]);
            }
        }
        EXPECT_EQ(0, stream.readableBytes());
        EXPECT_NO_THROW(stream.canRead(1));
    }

    TEST(MemoryStream, GetReadBufferUpdatesReadableBytes)
    {
        const std::size_t blockSize = 2;
        MemoryStream stream(blockSize);
        char testMsg[blockSize];
        stream.write(testMsg, blockSize);
        EXPECT_EQ(blockSize, stream.readableBytes());
        const std::size_t readSize = 1;
        stream.getReadBuffer(readSize);
        EXPECT_EQ(blockSize - readSize, stream.readableBytes());
    }

    TEST(MemoryStream, CannotGrowWithFixedCapacity)
    {
        const std::size_t blockSize = 2;
        const bool fixedCapacity = true;
        MemoryStream stream(blockSize, fixedCapacity);
        EXPECT_EQ(blockSize, stream.capacity());
        EXPECT_THROW(stream.grow(2 * blockSize), EndOfStreamException);
    }

    TEST(MemoryStream, CannotWriteUntilAllReadWithFixedCapacity)
    {
        const std::size_t blockSize = 2;
        const bool fixedCapacity = true;
        char testMsg[blockSize];
        MemoryStream stream(2 * blockSize, fixedCapacity);
        EXPECT_NO_THROW(stream.write(testMsg, blockSize));
        EXPECT_NO_THROW(stream.write(testMsg, blockSize));
        EXPECT_NO_THROW(stream.read(testMsg, blockSize));

        // blockSize bytes are still un-read, write will fail
        EXPECT_THROW(stream.write(testMsg, blockSize), EndOfStreamException);

        EXPECT_NO_THROW(stream.read(testMsg, blockSize));
        // All read, new writes are possible
        EXPECT_NO_THROW(stream.write(testMsg, blockSize));
        EXPECT_NO_THROW(stream.write(testMsg, blockSize));
        // stream is full, write will fail
        EXPECT_THROW(stream.write(testMsg, blockSize), EndOfStreamException);

        EXPECT_EQ(2 * blockSize, stream.capacity());
    }

}
