#include "DotIgnoreParser.h"

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

    bool anchored(std::string const& pattern) {
        auto pos = pattern.find_first_of("/");
        return (pos != std::string::npos && pos < pattern.length());
    }

    bool dirOnly(std::string const& pattern) {
        return pattern[pattern.length()] == '/';
    }

    std::string fwdSlashPath(std::filesystem::path const& patternPath) {
        bool toFwdSlash = (std::filesystem::path::preferred_separator == '\\');
        std::string pattern = patternPath.string();
        if (toFwdSlash) {
            std::replace(pattern.begin(), pattern.end(), '\\', '/');
        }
        return pattern;
    }
}

namespace YAM
{
    DotIgnoreRule::DotIgnoreRule(
        std::string const& pattern,
        std::string const& source // file + line nr
    )
        : _pattern(pattern)
        , _source(source)
        , _negate(pattern[0] == '!')
        , _anchored(anchored(pattern))
        , _dirOnly(dirOnly(pattern))
    {
        if (_negate) {
            _pattern = _pattern.substr(1, _pattern.length() - 1);
        }
        if (!_anchored) {
            _pattern.insert(0, "**/");
        }
        parse();
    }

    void DotIgnoreRule::parse() {
        try {
            Glob glob(fwdSlashPath(_pattern), true);
            _re = glob.regex();
        }
        catch (...) {
            throw std::runtime_error("Illegal pattern '" + _pattern + "' in " + _source);
        }
    }

    bool DotIgnoreRule::ignore(std::filesystem::path const& path) const {
        std::string fwdSlashedPath = fwdSlashPath(path);
        if (_anchored && fwdSlashedPath[0] != '/') {
            fwdSlashedPath.insert(0, "/");
        }
        std::smatch re_match;
        bool matches = std::regex_match(fwdSlashedPath, re_match, _re);
        return !_negate && matches;
    }

    std::string const& DotIgnoreRule::pattern() const { return _pattern; }
    std::string const& DotIgnoreRule::source() const { return _source; }
    bool DotIgnoreRule::negate() const { return _negate; }
    bool DotIgnoreRule::isAnchored() const { return _anchored; }
    bool DotIgnoreRule::isDirOnly() const { return _dirOnly; }
    std::regex const& DotIgnoreRule::re() const { return _re; }
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

    std::vector<DotIgnoreRule> const& DotIgnoreParser::rules() { return _rules; }
    bool DotIgnoreParser::hasNegations() const { return _hasNegations; }

    bool DotIgnoreParser::ignore(std::filesystem::path const& path) const
    {
        bool ignored = false;
        if (!_hasNegations) {
            for (const auto& rule : _rules) {
                if (rule.ignore(path)) {
                    ignored = true;
                    break;
                }
            }
        } else {
            for (const auto& rule : _rules) {
                if (rule.ignore(path)) {
                    ignored = true;
                } else if (rule.negate()) {
                    ignored = false;
                }
            }
        }
        return ignored;
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
