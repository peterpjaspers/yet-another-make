#include "ConsoleLogBook.h"
#include "Console.h"
#include <iostream>

namespace YAM
{        
    ConsoleLogBook::ConsoleLogBook()
        : BasicOStreamLogBook(&std::cout)
        , _console(std::make_shared<Console>())
    {}

    void ConsoleLogBook::setColors(LogRecord::Aspect aspect) {
        if (aspect == LogRecord::Error) {
            _console->textColor(IConsole::red);
        } else if (aspect == LogRecord::Warning) {
            _console->textColor(IConsole::orange);
        } else if (aspect == LogRecord::Progress) {
            _console->textColor(IConsole::green);
        }
    }

    void ConsoleLogBook::add(LogRecord const& record) {
        std::lock_guard<std::mutex> lock(_mutex);
        setColors(record.aspect);
        BasicOStreamLogBook::add(record);
        _console->restoreDefaultColors();
    }
}