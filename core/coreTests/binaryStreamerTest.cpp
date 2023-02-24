#include "../BinaryStreamer.h"
#include "../MemoryStream.h"
#include "../StreamableTypesBase.h"

#include <gtest/gtest.h>

namespace
{
    using namespace YAM;

    const unsigned int arrayCapacity = 10;


    class Streamable : public IStreamable
    {
    public:
        Streamable()
        {
            nBytes = arrayCapacity;
            for (unsigned int idx = 0; idx < nBytes; ++idx) {
                bytes[idx] = static_cast<char>(idx);
            }
            b = rand() % 2 == 0 ? true : false;
            f = static_cast<float>(rand() * 33.5);
            d = static_cast<double>(rand() * 56.9);
            i8 = static_cast<uint8_t>(rand() % (2 << 24));
            ui8 = static_cast<uint8_t>(rand() % (2 << 24));
            i16 = static_cast<int16_t>(rand() % (2 << 16));
            ui16 = static_cast<uint16_t>(rand() % (2 << 16));
            i32 = static_cast<int32_t>(rand());
            ui32 = static_cast<uint32_t>(rand());
            i64 = static_cast<uint64_t>(rand());
            ui64 = static_cast<uint64_t>(rand());
            str = std::string("dit is een test");
            wstr = std::wstring(L"dit is een wtest");
        }

        void AssertEqual(Streamable& other)
        {
            for (unsigned int idx = 0; idx < nBytes; ++idx) {
                EXPECT_EQ(bytes[idx], other.bytes[idx]);
            }
            EXPECT_EQ(b, other.b);
            EXPECT_EQ(f, other.f);
            EXPECT_EQ(d, other.d);
            EXPECT_EQ(i8, other.i8);
            EXPECT_EQ(ui8, other.ui8);
            EXPECT_EQ(i16, other.i16);
            EXPECT_EQ(ui16, other.ui16);
            EXPECT_EQ(i32, other.i32);
            EXPECT_EQ(ui32, other.ui32);
            EXPECT_EQ(i64, other.i64);
            EXPECT_EQ(ui64, other.ui64);
            EXPECT_EQ(str, other.str);
            EXPECT_EQ(wstr, other.wstr);
        }

        void stream(IStreamer* streamer) override
        {
            streamer->stream(static_cast<void*>(&bytes[0]), nBytes);
            streamer->stream(b);
            streamer->stream(f);
            streamer->stream(d);
            streamer->stream(i8);
            streamer->stream(ui8);
            streamer->stream(i16);
            streamer->stream(ui16);
            streamer->stream(i32);
            streamer->stream(ui32);
            streamer->stream(i64);
            streamer->stream(ui64);
            streamer->stream(str);
            streamer->stream(wstr);
        }

    private:
        uint32_t nBytes;
        char bytes[arrayCapacity];
        bool b;
        float f;
        double d;
        int8_t i8;
        uint8_t ui8;
        int16_t i16;
        uint16_t ui16;
        int32_t i32;
        uint32_t ui32;
        int64_t i64;
        uint64_t ui64;
        std::string str;
        std::wstring wstr;
    };

    class StreamableTypesById : public StreamableTypesByIdBase
    {
    protected:
        uint32_t getType(IStreamable* streamable) const override
        {
            if (streamable == nullptr) return INT_MAX;
            else if (dynamic_cast<Streamable*>(streamable) != nullptr) return 2;
            throw std::exception("unknown type");
        }

        IStreamable* createInstance(const unsigned int& typeId) const override
        {
            if (typeId == INT_MAX) return nullptr;
            if (typeId == 2) return new Streamable;
            throw std::exception("unknown type");
        }
    };

    class StreamableTypesByName : public StreamableTypesByNameBase
    {
    protected:
        std::string getType(IStreamable* streamable) const override
        {
            if (streamable == nullptr) return std::string("");
            else if (dynamic_cast<Streamable*>(streamable) != nullptr) return std::string("Streamable");
            throw std::exception("unknown type");
        }

        IStreamable* createInstance(std::string const& typeId) const override
        {
            if (typeId == std::string("")) return nullptr;
            if (typeId == std::string("Streamable")) return new Streamable;
            throw std::exception("unknown type");
        }
    };
}

namespace
{
    TEST(ValueStream, streamBasicTypes)
    {
        MemoryStream stream;
        Streamable written;
        BinaryWriter writer(&stream);
        written.stream(&writer);

        BinaryReader reader(&stream);
        Streamable read;
        read.stream(&reader);

        read.AssertEqual(written);
    }

    TEST(ValueStream, eos)
    {
        MemoryStream stream;
        Streamable written;
        BinaryWriter writer(&stream);
        written.stream(&writer);

        BinaryReader reader(&stream);
        Streamable read;
        read.stream(&reader);

        EXPECT_TRUE(reader.eos());
        EXPECT_THROW(read.stream(&reader), EndOfStreamException);
    }

    TEST(ValueStream, streamRawObjects)
    {
        StreamableTypesById types;
        MemoryStream stream;
        Streamable written;
        IStreamable* pwritten = &written;
        BinaryWriter writer(&types, &stream);
        writer.stream(&pwritten);
        writer.stream(&pwritten);

        BinaryReader reader(&types, &stream);
        IStreamable* read1;
        IStreamable* read2;
        reader.stream(&read1);
        reader.stream(&read2);

        EXPECT_TRUE(read1 == read2);
        Streamable* pread1 = dynamic_cast<Streamable*>(read1);
        EXPECT_NE(nullptr, pread1);
        written.AssertEqual(*pread1);
    }

    TEST(ValueStream, streamSharedObjects)
    {
        StreamableTypesByName types;
        MemoryStream stream;
        std::shared_ptr<Streamable> written = std::make_shared<Streamable>();
        std::shared_ptr<IStreamable> pwritten = written;
        BinaryWriter writer(&types, &stream);
        writer.stream(pwritten);
        writer.stream(pwritten);
        BinaryReader reader(&types, &stream);
        std::shared_ptr<IStreamable> read1;
        std::shared_ptr<IStreamable> read2;
        reader.stream(read1);
        reader.stream(read2);

        EXPECT_TRUE(read1 == read2);
        auto sread1 = dynamic_pointer_cast<Streamable>(read1);
        EXPECT_NE(nullptr, sread1);
        written.get()->AssertEqual(*(sread1.get()));
    }
}
