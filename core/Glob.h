#pragma once

#include <string>
#include <regex>
#include <filesystem>

namespace YAM {

    // A glob is a simplified regular expression:
    //  * matches 0, 1 or more characters
    //  ? matches 1 character
    //  ** matchs any path
    //  [abc] match character a or b or c
    //  {abc} group substring abc
    //  {abc},{def} match substring abc or def
    // 
    class __declspec(dllexport) Glob
    {
    public:

        // When globstar is true then normal meaning of **, else a consecutive
        // sequence of * symbols is collapsed to one *. E.g. ***/C becomes */C.
        //
        Glob(std::string const& globPattern, bool globstar);
        Glob(std::filesystem::path const& globPattern);

        // Return a regular expression pattern that represents the input globPattern.
        static std::string globPatternToRegex(std::string const& globPattern, bool globstar);

        // Return input path as a path with forward slash seperators
        static std::string fwdSlashPath(std::filesystem::path const& path);

        // Return whether pattern contains a glob special character.
        static bool isGlob(std::string const& pattern);
        static bool isGlob(std::filesystem::path const& pattern);

        bool matches(std::string const& str) const;
        bool matches(std::filesystem::path const& path) const;

        std::regex const& regex() const { return _re; }

    private:
        std::regex _re;
    };
}

