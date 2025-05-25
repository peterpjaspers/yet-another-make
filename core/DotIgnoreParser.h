#pragma once

#include "Glob.h"
#include "FileSystem.h"

#include <string>  
#include <regex>  

namespace YAM
{
    class DotIgnoreRule 
    {
    public:
        // Construct a rule from a non-empty, non-comment line from a file
        // whose syntax is as specified in https://git-scm.com/docs/gitignore.
        // 'source' typically contains storage location of pattern: file name
        // and line nr.
        DotIgnoreRule(std::string const& pattern, std::string const& source);

        // Return whether path is to be ignored, taking negate() into account.
        // Pre: path does not contain . and .. path components.
        bool ignore(std::filesystem::path const& path) const;

        // Return whether path matches the pattern, ignoring negate().
        // Pre: path does not contain . and .. path components.
        bool match(std::filesystem::path const& path) const;

        std::string const& pattern() const;
        std::string const& source() const;

        // Return whether pattern starts with '!'
        bool negate() const;

        // Return whether pattern contains '/' and '/' is not at end of pattern.
        bool isAnchored() const;

        // Return whether pattern ends with '/'
        bool isDirOnly() const;

        // Return the pattern (excluding a possibly leading !) as
        // regular expression 
        std::regex const& re() const;

    private:
        void parse();

        std::string _pattern;
        std::string _source;
        bool _negate;
        bool _anchored;
        bool _dirOnly;
        std::regex _re;
    };
    
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

