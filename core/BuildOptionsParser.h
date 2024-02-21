#pragma once

#include "BuildOptions.h"

namespace YAM
{
    struct __declspec(dllexport) BuildOptionsParser
    {
    public:
        BuildOptionsParser(int argc, char* argv[], BuildOptions& options);

        bool parseError() const { return _parseError; }

    private:
        bool _parseError;
    };
}
