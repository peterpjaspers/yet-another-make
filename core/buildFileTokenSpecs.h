#pragma once

#include "Token.h"
#include <regex>
#include <vector>

namespace YAM
{
    class __declspec(dllexport) TokenRegexSpec : public ITokenSpec {
    public:
        TokenRegexSpec(
            std::string const& pattern,
            std::string const& tokenType,
            std::size_t groupIndex = 0,
            bool skip = false);

        bool match(const char* str, Token& token) const override;
        bool skip() const;
        std::string const& type() const override;
        std::string const& pattern() const;

    private:
        std::string _pattern;
        std::regex _regex;
        std::string _type;
        std::size_t _group;
        bool _skip;
    };

    class __declspec(dllexport)  BuildFileTokenSpecs
    {
    public:
        static std::vector<ITokenSpec const*> const& ispecs();
        static std::vector<TokenRegexSpec const*> const& specs();

        static TokenRegexSpec const* whiteSpace();
        static TokenRegexSpec const* comment1();
        static TokenRegexSpec const* commentN();
        static TokenRegexSpec const* depBuildFile();
        static TokenRegexSpec const* depGlob();
        static TokenRegexSpec const* rule();
        static TokenRegexSpec const* foreach();
        static TokenRegexSpec const* ignore();
        static TokenRegexSpec const* curlyOpen();
        static TokenRegexSpec const* curlyClose();
        static TokenRegexSpec const* script();
        static TokenRegexSpec const* vertical();
        static TokenRegexSpec const* glob();
    };
}
