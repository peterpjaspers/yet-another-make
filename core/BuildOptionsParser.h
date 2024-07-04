#pragma once

#include "BuildOptions.h"

namespace YAM
{
    struct __declspec(dllexport) BuildOptionsParser
    {
    public:
        BuildOptionsParser(int argc, char* argv[], BuildOptions& options);

        bool parseError() const { return _parseError; }

        bool help() const { return _help; }
        bool noServer() const { return _noServer; }
        bool shutdown() const { return _shutdown; }

    private:
        bool _parseError;
        bool _help;
        bool _noServer;
        bool _shutdown;
    };
}
