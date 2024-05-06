#pragma once

#include "ILogBook.h"
#include "BasicOStreamLogBook.h"
#include <memory>
#include <mutex>

namespace YAM
{
    class Console;

    class __declspec(dllexport) ConsoleLogBook : public BasicOStreamLogBook
    {
    public:
        ConsoleLogBook();

        void add(LogRecord const& record) override;

    private:
        void setColors(LogRecord::Aspect aspect);

        std::mutex _mutex;
        std::shared_ptr<Console> _console;
    };
}


