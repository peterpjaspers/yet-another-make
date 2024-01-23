#include "TokenRegexSpec.h"

namespace YAM
{
    TokenRegexSpec::TokenRegexSpec(
        std::string const& pattern,
        std::string const& tokenType,
        std::size_t groupIndex,
        std::regex_constants::match_flag_type flags)
        : _pattern(pattern)
        , _regex(pattern)
        , _flags(flags)
        , _type(tokenType)
        , _group(groupIndex)
    {}

    bool TokenRegexSpec::match(const char* str, Token& token) const {
        token.spec = nullptr;
        std::cmatch cm;
        bool matched = std::regex_search(str, cm, _regex, _flags);
        if (matched) {
            token.spec = this;
            token.type = _type;
            token.consumed = cm[0].length();
            token.value = cm[_group].str();
        }
        return matched;
    }
}
