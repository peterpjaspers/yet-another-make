#pragma once
#include "Token.h"

namespace YAM
{
    class __declspec(dllexport)  BuildFileTokenSpecs
    {
    public:
        static ITokenSpec const* whiteSpace();
        static ITokenSpec const* comment1();
        static ITokenSpec const* commentN();
        static ITokenSpec const* depBuildFile();
        static ITokenSpec const* depGlob();
        static ITokenSpec const* rule();
        static ITokenSpec const* foreach();
        static ITokenSpec const* ignore();
        static ITokenSpec const* curlyOpen();
        static ITokenSpec const* curlyClose();
        static ITokenSpec const* script();
        static ITokenSpec const* vertical();
        static ITokenSpec const* glob();
    };
}
