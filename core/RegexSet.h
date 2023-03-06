#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <regex>

namespace YAM
{
    class __declspec(dllexport) RegexSet
    {
    public:
        // Construct an object that can match a string against a
        // set of regular expressions
        RegexSet() = default;

        // Return a portable (across Windows, Linus, MaxOs) regex string that matches
        // filesystem paths that contain given 'directory' component.
        // 'directory' must be a name, not a path, i.e. not contain directory 
        // seperators.
        static std::string matchDirectory(std::string const& directory);

        RegexSet(std::initializer_list<std::string> regexString);
        RegexSet(const RegexSet& other) = default;

        std::vector<std::string> const& regexStrings() const;

        void add(std::string const& regexString);
        void remove(std::string const& regexString);
        void clear();

        // Return whehter s matches one of the regular expressions.
        bool matches(std::string const & s) const;

    private:
        std::vector<std::string> _regexStrings;
        std::vector<std::regex> _regexes; // derived from _regexeStrings
    };
}
