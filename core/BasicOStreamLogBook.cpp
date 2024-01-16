#include "BasicOStreamLogBook.h"

#include <sstream>

namespace YAM
{
    BasicOStreamLogBook::BasicOStreamLogBook(std::basic_ostream<char, std::char_traits<char>>* ostream)
        : _ostream(ostream)
        , _startTime(std::chrono::system_clock::now())
    {}

    void BasicOStreamLogBook::add(LogRecord const& record)  {
        std::lock_guard<std::mutex> lock(_mutex);
        ILogBook::add(record);
        std::string timeStr;
        if (_logElapsedTime) {
            auto duration = record.time.time() - _startTime;
            std::chrono::milliseconds msd = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
            long long ms = msd.count();
            long long s = ms / 1000;
            ms -= s * 1000;
            std::stringstream ss;
            ss << "[" << s << "." << std::setfill('0') << std::setw(3) << ms << "s]";
            timeStr = ss.str();
        } else {
            timeStr = record.time.wctime().time3();
        }
        *_ostream
            << timeStr 
            << " " << LogRecord::aspect2str(record.aspect)
            << ": " << record.message << std::endl;
    } 
}