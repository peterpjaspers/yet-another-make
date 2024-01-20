#include "BuildFileTokenSpecs.h"
#include <regex>

namespace {
    using namespace YAM;

    TokenRegexSpec _whiteSpace(R"(^\s+)", "skip", 0, true);
    TokenRegexSpec _comment1(R"(^\/\/.*)", "skip", 0, true); // single-line comment
    TokenRegexSpec _commentN(R"(^\/\*[\s\S]*?\*\/)", "skip", 0, true); // multi-line comment
    TokenRegexSpec _depBuildFile(R"(^buildfile)", "depBuildFile");
    TokenRegexSpec _depGlob(R"(^glob)", "depGlob");
    TokenRegexSpec _rule(R"(^:)", "rule");
    TokenRegexSpec _foreach(R"(^foreach)", "foreach");
    TokenRegexSpec _ignore(R"(^\^)", "not");
    TokenRegexSpec _curlyOpen(R"(^\{)", "{");
    TokenRegexSpec _curlyClose(R"(^\})", "}");
    TokenRegexSpec _cmdDelim(R"(^\|>)", "_cmdDelim");
    TokenRegexSpec _script(R"(^\|>(((?!\|>)\S|\s)*)\|>)", "script", 1);
    TokenRegexSpec _vertical(R"(^\|[^>])", "|");
    TokenRegexSpec _glob(R"(^[^\{\}\|\s]+\\?(?:[\w\.\*\?\%\[\]-])+(?:\\([\w\.\*\?\%\[\]-])+)*)", "glob");

    std::vector<ITokenSpec const*> _tokenIspecs = {
        &_whiteSpace,
        &_comment1,
        &_commentN,
        &_depBuildFile,
        &_depGlob,
        &_rule,
        &_foreach,
        &_ignore,
        &_curlyOpen,
        &_curlyClose,
        &_script,
        &_vertical,
        &_glob,
    };

    std::vector<TokenRegexSpec const*> _tokenSpecs = {
        &_whiteSpace,
        &_comment1,
        &_commentN,
        &_depBuildFile,
        &_depGlob,
        &_rule,
        &_foreach,
        &_ignore,
        &_curlyOpen,
        &_curlyClose,
        &_script,
        &_vertical,
        &_glob,
    };
}

namespace YAM
{
    TokenRegexSpec::TokenRegexSpec(
        std::string const& pattern,
        std::string const& tokenType,
        std::size_t groupIndex,
        bool skip)
        : _pattern(pattern)
        , _regex(pattern)
        , _type(tokenType)
        , _group(groupIndex)
        , _skip(skip)
    {}

    bool TokenRegexSpec::match(const char* str, Token& token) const {
        token.spec = nullptr;
        std::cmatch cm;
        bool matched = std::regex_search(str, cm, _regex, std::regex_constants::match_continuous);
        if (matched) {
            token.spec = this;
            token.type = _type;
            token.consumed = cm[0].str();
            token.value = cm[_group].str();
            token.skip = _skip;
        }
        return matched;
    }

    bool TokenRegexSpec::skip() const { return _skip; }
    std::string const& TokenRegexSpec::type() const { return _type; }
    std::string const& TokenRegexSpec::pattern() const { return _pattern; }
}

namespace YAM
{
    std::vector<ITokenSpec const*> const& BuildFileTokenSpecs::ispecs() {
        return _tokenIspecs;
    }
    std::vector<TokenRegexSpec const*> const& BuildFileTokenSpecs::specs() {
        return _tokenSpecs;
    }

    TokenRegexSpec const* BuildFileTokenSpecs::whiteSpace() { return &_whiteSpace; }
    TokenRegexSpec const* BuildFileTokenSpecs::comment1() { return &_comment1; }
    TokenRegexSpec const* BuildFileTokenSpecs::commentN() { return &_commentN; }
    TokenRegexSpec const* BuildFileTokenSpecs::depBuildFile() { return &_depBuildFile; }
    TokenRegexSpec const* BuildFileTokenSpecs::depGlob() { return &_depGlob; }
    TokenRegexSpec const* BuildFileTokenSpecs::rule() { return &_rule; }
    TokenRegexSpec const* BuildFileTokenSpecs::foreach() { return &_foreach; }
    TokenRegexSpec const* BuildFileTokenSpecs::ignore() { return &_ignore; }
    TokenRegexSpec const* BuildFileTokenSpecs::curlyOpen() { return &_curlyOpen; }
    TokenRegexSpec const* BuildFileTokenSpecs::curlyClose() { return &_curlyClose; }
    TokenRegexSpec const* BuildFileTokenSpecs::script() { return &_script; }
    TokenRegexSpec const* BuildFileTokenSpecs::vertical() { return &_vertical; }
    TokenRegexSpec const* BuildFileTokenSpecs::glob() { return &_glob; }
}
