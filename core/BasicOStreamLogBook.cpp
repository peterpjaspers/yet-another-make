#include "BasicOStreamLogBook.h"


namespace YAM
{
    BasicOStreamLogBook::BasicOStreamLogBook(std::basic_ostream<char, std::char_traits<char>>* ostream)
        : _ostream(ostream)
    {}

    void BasicOStreamLogBook::add(LogRecord const& record)  {
        std::lock_guard<std::mutex> lock(_mutex);
        ILogBook::add(record);
        *_ostream
            << record.time.wctime().time3() 
            << " " << LogRecord::aspect2str(record.aspect)
            << ": " << record.message << std::endl;
    } 
}