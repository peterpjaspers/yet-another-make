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
        // When globstar is false then a consecutive sequence of
        // * symbols is collapsed to 1. E.g. B/**/C becomes B/*/C.
        // Given flags is used in call to std::regex_match.
        Glob(std::string const& globPattern, bool globstar);

        bool matches(std::string const& str) const;
        bool matches(std::filesystem::path const& path) const;

    private:
        std::regex _re;
    };
}

