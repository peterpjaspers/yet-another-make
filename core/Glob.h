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
        Glob(std::filesystem::path const& globPattern);

        // Return whether patterns contains glob special characters.
        // I.e. one or more of * ? [] {} ,
        static bool isGlob(std::string const& pattern);

        bool matches(std::string const& str) const;
        bool matches(std::filesystem::path const& path) const;

    private:
        std::regex _re;
    };
}

