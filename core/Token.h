#pragma once

#include <string>

namespace YAM {
    class ITokenSpec;

    struct __declspec(dllexport) Token {
        Token() : spec(nullptr), skip(false) {}
        ITokenSpec const* spec;
        std::string type;
        std::string value; // the string that matches spec.
        std::string consumed; // value is substring of consumed
        bool skip; // whether to skip this token and return the next token instead
    };

    class __declspec(dllexport) ITokenSpec {
    public:
        virtual bool match(const char* str, Token& token) const = 0;
        // Return string that identifies the token class.
        virtual std::string const& type() const = 0;
    };
}