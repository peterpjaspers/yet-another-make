#include "../BinaryValueStreamer.h"
#include "../SharedObjectStreamer.h"
#include "../ObjectStreamer.h"
#include "../Streamer.h"
#include "../IStreamable.h"
#include "../MemoryStream.h"

#include <gtest/gtest.h>

namespace
{
    using namespace YAM;

    const unsigned int arrayCapacity = 10;

    class Streamable : public IStreamable
    {
    public:
        Streamable() {
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
            path = std::filesystem::path(R"(aap/noot/mies)");
        }

        Streamable(IStreamer* streamer) {
            stream(streamer);
        }

        void assertEqual(Streamable& other) {
            EXPECT_EQ(nBytes, other.nBytes);
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
            EXPECT_EQ(path.string(), other.path.string());
        }

        void stream(IStreamer* streamer) override {
            streamer->stream(nBytes);
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
            streamer->stream(path);
        }

        uint32_t typeId() const override { return 2; }

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
        std::filesystem::path path;
    };


    class Streamable1 : public Streamable
    {
    public:
        Streamable1() : Streamable() {};
        Streamable1(IStreamer* streamer) : Streamable(streamer) {};

        uint32_t typeId() const override { return 3; }
    };

    class StreamableReader : public ObjectReader
    {
    protected:
        IStreamable* readObject(IStreamer* streamer, uint32_t typeId) const override
        {
            if (typeId == INT_MAX) return nullptr;
            if (typeId == 2) return new Streamable(streamer);
            if (typeId == 3) return new Streamable1(streamer);
            throw std::exception("unknown type");
        }
    };

    class StreamableWriter : public ObjectWriter
    {
        uint32_t getTypeId(IStreamable* object) const override {
            return object->typeId();
        }
    };

    class StreamerSetup
    {
    public:
        StreamerSetup()
            : _valueWriter(&_stream)
            , _valueReader(&_stream)
            , _sharedObjectWriter(&_objectWriter)
            , _sharedObjectReader(&_objectReader)
            , _writer(&_valueWriter, &_sharedObjectWriter)
            , _reader(&_valueReader, &_sharedObjectReader)
        {}

        Streamer* writer() { return &_writer; }
        Streamer* reader() { return &_reader; }

    private:
        MemoryStream _stream;
        BinaryValueWriter _valueWriter;
        BinaryValueReader _valueReader;
        StreamableWriter _objectWriter;
        StreamableReader _objectReader;
        SharedObjectWriter _sharedObjectWriter;
        SharedObjectReader _sharedObjectReader;
        Streamer _writer;
        Streamer _reader;
    };
}

namespace
{
    TEST(Streamer, streamBasicTypes) {
        StreamerSetup setup;
        Streamable expected;
        expected.stream(setup.writer());

        Streamable actual;
        actual.stream(setup.reader());
        expected.assertEqual(actual);
    }

    TEST(Streamer, eos) {
        StreamerSetup setup;
        Streamable expected;
        expected.stream(setup.writer());
        Streamable actual;
        actual.stream(setup.reader());

        EXPECT_THROW(actual.stream(setup.reader()), EndOfStreamException);
    }
    
    TEST(Streamer, streamSharedObjects) {
        StreamerSetup setup;
        std::shared_ptr<Streamable> expected0 = std::make_shared<Streamable>();
        std::shared_ptr<IStreamable> pexpected0 = expected0;
        std::shared_ptr<Streamable> expected1 = std::make_shared<Streamable1>();
        std::shared_ptr<IStreamable> pexpected1 = expected1;
        setup.writer()->stream(pexpected0);
        setup.writer()->stream(pexpected0);
        setup.writer()->stream(pexpected1);
        setup.writer()->stream(pexpected1);
        std::shared_ptr<IStreamable> actual01;
        std::shared_ptr<IStreamable> actual02;
        std::shared_ptr<IStreamable> actual11;
        std::shared_ptr<IStreamable> actual12;
        setup.reader()->stream(actual01);
        setup.reader()->stream(actual02);
        setup.reader()->stream(actual11);
        setup.reader()->stream(actual12);

        EXPECT_TRUE(actual01 == actual02);
        auto sactual01 = dynamic_pointer_cast<Streamable>(actual01);
        EXPECT_NE(nullptr, sactual01);
        expected0.get()->assertEqual(*(sactual01.get()));

        EXPECT_TRUE(actual11 == actual12);
        auto sactual11 = dynamic_pointer_cast<Streamable1>(actual11);
        EXPECT_NE(nullptr, sactual11);
        expected1.get()->assertEqual(*(sactual11.get()));
    }

    TEST(Streamer, streamIntVector) {
        StreamerSetup setup;
        std::vector<int> expected = { 1,2,3,4 };
        setup.writer()->streamVector(expected);
        std::vector<int> actual;
        setup.reader()->streamVector(actual);
        EXPECT_EQ(expected, actual);
    }

    TEST(Streamer, streamObjectVector) {
        StreamerSetup setup;
        std::shared_ptr<Streamable> expected = std::make_shared<Streamable>();
        std::shared_ptr<IStreamable> pexpected = expected;
        std::shared_ptr<Streamable> expected1 = std::make_shared<Streamable1>();
        std::shared_ptr<IStreamable> pexpected1 = expected1;
        std::vector<std::shared_ptr<IStreamable>> expecteds = { pexpected, pexpected1, pexpected, pexpected1 };
        setup.writer()->streamVector(expecteds);
        std::vector<std::shared_ptr<IStreamable>> actuals;
        setup.reader()->streamVector(actuals);
        EXPECT_EQ(expecteds.size(), actuals.size());
        EXPECT_EQ(actuals[0].get(), actuals[2].get());
        EXPECT_EQ(actuals[1].get(), actuals[3].get());
        auto actual0 = dynamic_pointer_cast<Streamable>(actuals[0]);
        expected.get()->assertEqual(*actual0);
        auto actual1 = dynamic_pointer_cast<Streamable1>(actuals[1]);
        expected1.get()->assertEqual(*actual1);
    }

    TEST(Streamer, streamObjectMap) {
        StreamerSetup setup;
        const std::string s1("streamable1");
        const std::string s2("streamable2");
        std::shared_ptr<Streamable> expected = std::make_shared<Streamable>();
        std::shared_ptr<IStreamable> pexpected = expected;
        std::map<std::string, std::shared_ptr<IStreamable>> expecteds = {
            {s1, pexpected}, { s2, pexpected} };
        setup.writer()->streamMap(expecteds);
        std::map<std::string, std::shared_ptr<IStreamable>> actuals;
        setup.reader()->streamMap(actuals);
        EXPECT_EQ(expecteds.size(), actuals.size());
        auto actual1 = dynamic_pointer_cast<Streamable>(actuals[s1]);
        auto actual2 = dynamic_pointer_cast<Streamable>(actuals[s2]);
        EXPECT_EQ(actual1.get(), actual2.get());
        expected.get()->assertEqual(*actual1);
    }

}
