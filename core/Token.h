#pragma once

#include <string>

namespace YAM {
    class ITokenSpec;

    struct __declspec(dllexport) Token {
        Token() : spec(nullptr), consumed(0) {}
        ITokenSpec const* spec;
        // Next three members only valid when spec != nullptr
        std::string type;
        std::string value; // the string that matches spec.
        std::size_t consumed; // nr of characters consumed: >= value.length()
    };

    class __declspec(dllexport) ITokenSpec {
    public:
        virtual bool match(const char* str, Token& token) const = 0;
    };
}