#pragma once

#include "LogRecord.h"

namespace YAM
{
    class __declspec(dllexport) ILogBook
    {
    public:
        ILogBook() : _error(false), _aspects(LogRecord::allAspects()) {}

        virtual void add(LogRecord const& record) {
            if (record.aspect == LogRecord::Aspect::Error) _error = true;
        }

        // Return whether an error record has been logged
        virtual bool error() const { return _error; }

        // Post: !error();
        virtual void resetError() { _error = false; }

        // Set the aspects that must be logged
        void setAspects(std::vector<LogRecord::Aspect> aspects) {
            _aspects = aspects;
        }

        std::vector<LogRecord::Aspect> const& getAspects(std::vector<LogRecord::Aspect>) {
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
