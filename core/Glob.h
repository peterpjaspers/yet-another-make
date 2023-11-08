#pragma once

#include <string>
#include <regex>
#include <filesystem>

namespace YAM {

    class __declspec(dllexport) Glob
    {
    public:
        // When globstar is true then 
        //     /**/ means to recurse all directories below root
        //     B/**/ means to recurse all directories below B
        // When globstar is false then a consecutive sequence of * symbols 
        // collapsed to one *. E.g. B/**/C becomes B/*/C.
        //
        Glob(std::string const& globPattern, bool globstar);

        bool matches(std::string const& str) const;
        bool matches(std::filesystem::path const& path) const;

        // Return whether the glob is a literal, i.e. a string
        // without glob special characters (* ? [] {} ,)
        bool isLiteral() const { return _re.second; }

    private:
        std::pair<std::regex, bool> _re;
    };
}

