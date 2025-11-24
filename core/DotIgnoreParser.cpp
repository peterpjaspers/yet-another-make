#include "DotIgnoreParser.h"
#include "IStreamer.h"

#include <fstream>

namespace
{
    std::string readFile(std::filesystem::path path) {
        std::ifstream file(path.string());
        std::string line, content;
        while (getline(file, line)) content += line;
        file.close();
        return content;
    }

    std::string trim(std::string const& str) {
        const std::string whitespace = " \t";

        const auto strBegin = str.find_first_not_of(whitespace);
        if (strBegin == std::string::npos) {
            return ""; // no content
        } else {
            const auto strEnd = str.find_last_not_of(whitespace);
            const auto strRange = strEnd - strBegin + 1;
            return str.substr(strBegin, strRange);
        }
    }

    // Return whether pattern contains '/' but not only as last character. 
    // Note: '/' as last character means that pattern must match directory.
    bool anchored(std::string const& pattern) {
        auto pos = pattern.find_first_of("/");
        return (pos != std::string::npos && pos != pattern.length()-1);
    }

    bool dirOnly(std::string const& pattern) {
        return pattern[pattern.length()-1] == '/';
    }
}

namespace YAM
{
    DotIgnoreParser::DotIgnoreParser(std::filesystem::path const& ignoreFile)
        : _hasNegations(false)
    {
        std::ifstream stream(ignoreFile.string());
        parseStream(ignoreFile, stream);
        stream.close();
    }

    DotIgnoreParser::DotIgnoreParser(std::filesystem::path const& ignoreFile, std::string const& fileContent)
        : _hasNegations(false)
    {
        std::istringstream stream(fileContent);
        parseStream(ignoreFile, stream);
    }

    std::vector<DotIgnoreRule> const& DotIgnoreParser::rules() const { return _rules; }
    bool DotIgnoreParser::hasNegations() const { return _hasNegations; }

    bool DotIgnoreParser::ignore(std::filesystem::path const& path) const
    {
        for (auto it = std::rbegin(_rules); it != std::rend(_rules); ++it) {
            auto rule = *it;
            if (rule.match(path)) {
                return !rule.negate();
            }
        }
        return false;
    }

    void DotIgnoreParser::parseStream(std::filesystem::path const& ignoreFile, std::basic_istream<char>& stream) {
        std::string line;
        int lineNr = 0;
        while (std::getline(stream, line)) {
            lineNr += 1;
            std::string trimmedLine = trim(line);
            if (!trimmedLine.empty() && trimmedLine[0] != '#') {
                if (trimmedLine[0] == '!') _hasNegations = true;
                DotIgnoreRule rule(
                    trimmedLine,
                    ignoreFile.string() + std::to_string(lineNr));
                _rules.push_back(rule);
            }
        }
    }

    void DotIgnoreParser::parseLine(std::string const& origLine) {
        std::string line = origLine;
        bool negate = false;
        if (line[0] == '!') {
            line = line.substr(1);
            negate = true;
            _hasNegations = true;
        }
        // TODO
    }
}
