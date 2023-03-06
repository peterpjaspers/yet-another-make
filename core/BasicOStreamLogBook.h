#pragma once

#include "ILogBook.h"
#include <ostream>
#include <mutex>

namespace YAM
{
    class __declspec(dllexport) BasicOStreamLogBook : public ILogBook
    {
    public:
        BasicOStreamLogBook(std::basic_ostream<char, std::char_traits<char>> *ostream);

        void add(LogRecord const& record) override;

    private:
        std::mutex _mutex;
        std::basic_ostream<char, std::char_traits<char>>* _ostream;
    };
}

