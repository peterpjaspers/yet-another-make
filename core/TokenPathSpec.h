#pragma once
#include "Token.h"

namespace YAM
{
    class __declspec(dllexport) TokenPathSpec : public ITokenSpec {

        // Match path, glob, group
        // Return matched type in token.type.
        //
        bool match(const char* str, Token& token) const override;
    };
}

