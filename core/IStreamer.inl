#include "IStreamable.h"
#include "TimePoint.h"

#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>

namespace
{
    using namespace YAM;

    template<typename Str>
    void writeString(IStreamer* streamer, Str& string) {
        typedef Str::value_type VT;
        uint32_t nChars = static_cast<uint32_t>(string.length());
        uint32_t nBytes = nChars * sizeof(VT);
        streamer->stream(nBytes);
        const void* cstr = string.c_str();
        streamer->stream(const_cast<void*>(cstr), nBytes);
    }

    template<typename Str>
    void readString(IStreamer* streamer, Str& string) {
        typedef Str::value_type VT;
        uint32_t nBytes;
        streamer->stream(nBytes);
        uint32_t nChars = nBytes / sizeof(VT);
        std::unique_ptr<VT> buf(new VT[nChars]);
        streamer->stream(buf.get(), nBytes);
        string.clear();
        string.insert(0, buf.get(), nChars);
    }

    void writePath(IStreamer* streamer, std::filesystem::path& path) {
        std::string str = path.string();
        writeString(streamer, str);
    }

    void readPath(IStreamer* streamer, std::filesystem::path& path) {
        std::string str;
        readString(streamer, str);
        path = std::filesystem::path(str);
    }

    void writeTimePoint(IStreamer* streamer, std::chrono::system_clock::time_point& tpc) {
        TimePoint tp(tpc);
        std::string tps = tp.wctime().dateTime();
        writeString(streamer, tps);
    }

    void readTimePoint(IStreamer* streamer, std::chrono::system_clock::time_point& tpc) {
        std::string tps;
        readString(streamer, tps);
        WallClockTime wct(tps);
        TimePoint tp(wct);
        tpc = tp.time();
    }

    void writeUtcTimePoint(IStreamer* streamer, std::chrono::utc_clock::time_point& tpc) {
        unsigned long long cnt = tpc.time_since_epoch().count();
        streamer->stream(cnt);
    }
    void readUtcTimePoint(IStreamer* streamer, std::chrono::utc_clock::time_point& tpc) {
        unsigned long long cnt;
        streamer->stream(cnt);
        std::chrono::utc_clock::duration d(cnt);
        tpc = std::chrono::utc_clock::time_point(d);
    }

    template <typename T>
    void writeVector(IStreamer* streamer, std::vector<T>& items)
    {
        const std::size_t length = items.size();
        if (length > UINT_MAX) throw std::exception("vector too large");
        uint32_t nItems = static_cast<uint32_t>(length);
        streamer->stream(nItems);
        for (auto item : items) {
            streamer->stream(item);
        }
    }

    template <typename T>
    void readVector(IStreamer* streamer, std::vector<T>& items)
    {
        items.clear();
        uint32_t nItems;
        streamer->stream(nItems);
        for (uint32_t i = 0; i < nItems; ++i) {
            T item;
            streamer->stream(item);
            items.push_back(item);
        }
    }

    template <typename K, typename V>
    void writeMap(IStreamer* streamer, std::map<K,V>& items)
    {
        const std::size_t length = items.size();
        if (length > UINT_MAX) throw std::exception("map too large");
        uint32_t nItems = static_cast<uint32_t>(length);
        streamer->stream(nItems);
        for (auto const& pair : items) {
            K key = pair.first;
            V value = pair.second;
            streamer->stream(key);
            streamer->stream(value);
        }
    }

    template <typename K, typename V>
    void readMap(IStreamer* streamer, std::map<K,V>& items)
    {
        items.clear();
        uint32_t nItems;
        streamer->stream(nItems);
        for (uint32_t i = 0; i < nItems; ++i) {
            std::pair<K,V> pair;
            streamer->stream(pair.first);
            streamer->stream(pair.second);
            items.insert(pair);
        }
    }
}

namespace YAM
{
    inline void IStreamer::stream(std::string& str) {
        if (writing()) {
            writeString(this, str);
        } else {
            readString(this, str);
        }
    }

    inline void IStreamer::stream(std::wstring& str) {
        if (writing()) {
            writeString(this, str);
        } else {
            readString(this, str);
        }
    }

    inline void IStreamer::stream(std::filesystem::path& path) {
        if (writing()) {
            writePath(this, path);
        } else {
            readPath(this, path);
        }
    }

    inline void IStreamer::stream(std::chrono::system_clock::time_point& tp) {
        if (writing()) {
            writeTimePoint(this, tp);
        } else {
            readTimePoint(this, tp);
        }
    }

    inline void IStreamer::stream(std::chrono::utc_clock::time_point& tp) {
        if (writing()) {
            writeUtcTimePoint(this, tp);
        } else {
            readUtcTimePoint(this, tp);
        }
    }

    template <typename T>
    inline void IStreamer::streamVector(std::vector<T>& items) {
        if (writing()) {
            writeVector(this, items);
        } else {
            readVector(this, items);
        }
    }

    template <typename K, typename V>
    inline void IStreamer::streamMap(std::map<K,V>& items) {
        if (writing()) {
            writeMap(this, items);
        } else {
            readMap(this, items);
        }
    }

    template <typename T>
    inline void IStreamer::stream(std::shared_ptr<T>& item) {
        std::shared_ptr<IStreamable> s;
        if (writing()) {
            s = dynamic_pointer_cast<IStreamable>(item);
        }
        stream(s);
        if (reading()) {
            item = dynamic_pointer_cast<T>(s);
        }
    }

}
