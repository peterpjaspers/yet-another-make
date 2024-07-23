#pragma once

#include "LogRecord.h"

namespace YAM
{
    class __declspec(dllexport) ILogBook
    {
    public:
        ILogBook() : 
            _error(false),
            _aspects({ LogRecord::Aspect::Error, LogRecord::Aspect::Warning, LogRecord::Aspect::Progress }) 
        {}

        virtual void add(LogRecord const& record) {
            if (record.aspect == LogRecord::Aspect::Error) _error = true;
        }

        // Return whether an error record has been logged
        virtual bool error() const { return _error; }

        // Post: !error();
        virtual void resetError() { _error = false; }

        void aspects(std::vector<LogRecord::Aspect> aspects) {
            _aspects = aspects;
        }

        std::vector<LogRecord::Aspect> const& aspects() {
            return _aspects;
        }

        // Return whether aspect must be logged
        bool mustLogAspect(LogRecord::Aspect aspect) {
            return std::find(_aspects.begin(), _aspects.end(), aspect) != _aspects.end();
        }

    protected:
        bool _error;
        std::vector<LogRecord::Aspect> _aspects;
    };
}
