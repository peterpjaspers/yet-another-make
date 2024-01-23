#include "BuildFileTokenSpecs.h"
#include <regex>

namespace {
    using namespace YAM;

    TokenRegexSpec _whiteSpace(R"(^\s+)", "'skip'whitespace", 0);
    TokenRegexSpec _comment1(R"(^\/\/.*)", "comment1", 0); // single-line comment
    TokenRegexSpec _commentN(R"(^\/\*[\s\S]*?\*\/)", "commentN", 0); // multi-line comment
    TokenRegexSpec _depBuildFile(R"(^buildfile)", "depBuildFile");
    TokenRegexSpec _depGlob(R"(^glob)", "depGlob");
    TokenRegexSpec _rule(R"(^:)", "rule");
    TokenRegexSpec _foreach(R"(^foreach)", "foreach");
    TokenRegexSpec _ignore(R"(^\^)", "not");
    TokenRegexSpec _curlyOpen(R"(^\{)", "{");
    TokenRegexSpec _curlyClose(R"(^\})", "}");
    TokenRegexSpec _cmdStart(R"(^\|>)", "cmdStart");
    TokenRegexSpec _cmdEnd(R"(\|>)", "cmdEnd", 0, std::regex_constants::match_default);
    TokenRegexSpec _script(R"(^\|>(((?!\|>)\S|\s)*)\|>)", "script", 1);
    TokenRegexSpec _vertical(R"(^\|[^>])", "|");
    TokenPathSpec _glob;

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
}

namespace YAM
{
    ITokenSpec const* BuildFileTokenSpecs::whiteSpace() { return &_whiteSpace; }
    ITokenSpec const* BuildFileTokenSpecs::comment1() { return &_comment1; }
    ITokenSpec const* BuildFileTokenSpecs::commentN() { return &_commentN; }
    ITokenSpec const* BuildFileTokenSpecs::depBuildFile() { return &_depBuildFile; }
    ITokenSpec const* BuildFileTokenSpecs::depGlob() { return &_depGlob; }
    ITokenSpec const* BuildFileTokenSpecs::rule() { return &_rule; }
    ITokenSpec const* BuildFileTokenSpecs::foreach() { return &_foreach; }
    ITokenSpec const* BuildFileTokenSpecs::ignore() { return &_ignore; }
    ITokenSpec const* BuildFileTokenSpecs::curlyOpen() { return &_curlyOpen; }
    ITokenSpec const* BuildFileTokenSpecs::curlyClose() { return &_curlyClose; }
    ITokenSpec const* BuildFileTokenSpecs::cmdStart() { return &_cmdStart; }
    ITokenSpec const* BuildFileTokenSpecs::cmdEnd() { return &_cmdEnd; }
    ITokenSpec const* BuildFileTokenSpecs::script() { return &_script; }
    ITokenSpec const* BuildFileTokenSpecs::vertical() { return &_vertical; }
    ITokenSpec const* BuildFileTokenSpecs::glob() { return &_glob; }
}
