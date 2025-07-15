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
    DotIgnoreRule::DotIgnoreRule(
        std::string const& pattern,
        std::string const& source // file + line nr
    )
        : _pattern(pattern)
        , _negate(pattern[0] == '!')
        , _anchored(anchored(pattern))
        , _dirOnly(dirOnly(pattern))
    {
        if (_negate) {
            _pattern = _pattern.substr(1, _pattern.length() - 1);
        }
        parse(source);
    }

    void DotIgnoreRule::parse(std::string const& source) {
        auto fwdGlobPattern = Glob::fwdSlashPath(_pattern);
        // Check whether pattern is of form "*.ext", i.e. whether
        // patterns matches path extension ".ext".
        static std::regex extRe("^\\*(\\.[^\\*\\?\\[\\]]*)$");
        std::smatch extMatch;
        if (std::regex_match(fwdGlobPattern, extMatch, extRe)) {
            _extension = extMatch[1].str();
        }
        if (_extension.empty()) {
            if (_anchored) {
                if (fwdGlobPattern[0] != '/') {
                    fwdGlobPattern.insert(0, "/");
                }
            } else {
                fwdGlobPattern.insert(0, "**/");
            }
            try {
                _reString = Glob::globPatternToRegex(fwdGlobPattern, true);
                _re = std::regex(_reString, std::regex::optimize);
            } catch (...) {
                throw std::runtime_error("Illegal pattern '" + _pattern + "' in " + source);
            }
        }
    }

    bool DotIgnoreRule::ignore(std::filesystem::path const& path) const {
        return !_negate && match(path);
    }

    bool DotIgnoreRule::match(std::filesystem::path const& path) const {
        if (_extension.empty()) {
            std::string fwdSlashedPath = Glob::fwdSlashPath(path);
            if (_anchored && fwdSlashedPath[0] != '/') {
                fwdSlashedPath.insert(0, "/");
            }
            std::smatch re_match;
            bool matches = std::regex_match(fwdSlashedPath, re_match, _re);
            return matches;
        } else {
            return path.extension() == _extension;
        }
    }

    std::string const& DotIgnoreRule::pattern() const { return _pattern; }
    bool DotIgnoreRule::negate() const { return _negate; }
    bool DotIgnoreRule::isAnchored() const { return _anchored; }
    bool DotIgnoreRule::isDirOnly() const { return _dirOnly; }
    std::string const& DotIgnoreRule::reString() const { return _reString; }
    std::regex const& DotIgnoreRule::re() const { return _re; }

    void DotIgnoreRule::streamVector(
        IStreamer* streamer,
        std::vector<DotIgnoreRule>& rules
    ) {
        uint32_t cnt;
        if (streamer->writing()) cnt = static_cast<uint32_t>(rules.size());
        streamer->stream(cnt);
        if (streamer->reading()) rules = std::vector<DotIgnoreRule>(cnt);
        for (uint32_t i = 0; i < cnt; i++) rules[i].stream(streamer);
    }

    void DotIgnoreRule::stream(IStreamer* streamer) {
        streamer->stream(_pattern);
        streamer->stream(_negate);
        streamer->stream(_anchored);
        streamer->stream(_dirOnly);
        if (streamer->reading()) {
            parse("");
        }
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
