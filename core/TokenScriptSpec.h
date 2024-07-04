#pragma once
#include "Token.h"

namespace YAM
{
    class __declspec(dllexport) TokenScriptSpec : public ITokenSpec {

        // Match rule script
        bool match(const char* str, Token& token) const override;
    };
}

