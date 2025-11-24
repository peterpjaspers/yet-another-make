#pragma once

#include "DotIgnoreRule.h"
#include "Glob.h"
#include "FileSystem.h"
#include "IStreamable.h"
#include "xxhash.h"

#include <string>  
#include <regex>  

namespace YAM
{
    class DotIgnoreParser
    {
    public:
        DotIgnoreParser(std::filesystem::path const& ignoreFile);
        DotIgnoreParser(std::filesystem::path const& ignoreFile, std::string const& fileContent);

        // Return the parsed rules, in order of appearance in the file.
        std::vector<DotIgnoreRule> const& rules() const;

        // Return whether the file contains negated rules.
        bool hasNegations() const;

        // Return whether given path is to be ignored.
        // Note: this function does not handle the situation where path is in a
        // previously ignored directory and a negate rule matches path. In such
        // cases this function returns false while it should return true.
        bool ignore(std::filesystem::path const& path) const;

    private:
        void parseStream(std::filesystem::path const& ignoreFile, std::basic_istream<char>& stream);
        void parseLine(std::string const& origLine);

        std::vector<DotIgnoreRule> _rules;
        bool _hasNegations;
    };
}

