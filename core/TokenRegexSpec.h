#pragma once
#include "Token.h"

#include <regex>

namespace YAM
{
    class __declspec(dllexport) TokenRegexSpec : public ITokenSpec {
    public:
        TokenRegexSpec(
            std::string const& pattern,
            std::string const& tokenType,
            std::size_t groupIndex = 0,
            std::regex_constants::match_flag_type flags = std::regex_constants::match_continuous
        );

        bool match(const char* str, Token& token) const override;

    private:
        std::string _pattern;
        std::regex _regex;
        std::regex_constants::match_flag_type _flags;
        std::string _type;
        std::size_t _group;
        bool _skip;
    };
}

