#include "BasicOStreamLogBook.h"

#include <sstream>

namespace YAM
{
    BasicOStreamLogBook::BasicOStreamLogBook(std::basic_ostream<char, std::char_traits<char>>* ostream)
        : _ostream(ostream)
        , _startTime(std::chrono::system_clock::now())
        , _logElapsedTime(false)
        , _logAspect(false)
    {}

    void BasicOStreamLogBook::add(LogRecord const& record)  {
        if (!mustLogAspect(record.aspect)) return;

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
        *_ostream << timeStr << " ";
        if (_logAspect) *_ostream << LogRecord::aspect2str(record.aspect) << ": ";
        *_ostream << record.message << std::endl;
    } 
}