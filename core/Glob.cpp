#include "Glob.h"

#include <sstream>

namespace {

    std::regex globPatternAsRegex(std::string const& input, bool globstar) {
        bool inGroup = false;
        std::stringstream ss;
        ss << '^';
        for (std::size_t i = 0; i < input.length(); i++) {
            char c = input[i];
            switch (c) {
            //case '/':
            case '$':
            case '^':
            case '+':
            case '.':
            case '(':
            case ')':
            case '=':
            case '!':
            case '|': {
                ss << R"(\)" << c;
                break;
            }

            case '?': {
                ss << '.';
                break;
            }

            case '[':
            case ']': {
                ss << c;
                break;
            }

            case '{': {
                inGroup = true;
                ss << '(';
                break;
            }

            case '}': {
                inGroup = false;
                ss << ')';
                break;
            }

            case ',': {
                if (inGroup) {
                    ss << '|';
                    break;
                }
                ss << "\\" << c;
                break;
            }

            case '*': {
                 // Move over all consecutive * chars and store the
                 // the previous and next characters
                char prevChar = (i - 1) < input.length() ? input[i - 1] : '\0';
                std::size_t starCount = 1;
                while ((i + 1) < input.length() && input[i + 1] == '*') {
                    starCount++;
                    i++;
                }
                char nextChar = (i + 1) < input.length() ? input[i + 1] : '\0';

                if (!globstar) {
                    ss << ".*";
                } else {
                    bool isGlobstar = starCount > 1                // multiple '*' characters
                        && (prevChar == '/' || prevChar == '\0')   // from the start of the segment
                        && (nextChar == '/' || nextChar == '\0');  // to the end of the segment

                    if (isGlobstar) {
                        // it's a globstar, so match zero or more path segments
                        ss << R"(((?:[^/]*(?:\/|$))*))";
                        i++; // move over the /
                    } else {
                        // it's not a globstar, so only match one path segment
                        ss << R"(([^/]*))";
                    }
                }
                break;
            }

            default:
                ss << c;
            }
        }
        ss << '$';
        return std::regex(ss.str());
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

namespace YAM {

    Glob::Glob(std::string const& globPattern, bool globstar)
        : _re(globPatternAsRegex(fwdSlashPath(globPattern), globstar))
    {}

    Glob::Glob(std::filesystem::path const& globPattern)
        : _re(globPatternAsRegex(fwdSlashPath(globPattern), true))
    {}

    bool Glob::isGlob(std::string const& pattern) {
        // {} characters removed because not allowed in yam buildfiles.
        static const auto check = std::regex(R"(([\*\?\[\]]))");
        return std::regex_search(pattern, check);
    }

    bool Glob::matches(std::string const& str) const {
        return std::regex_match(str, _re);
    }

    bool Glob::matches(std::filesystem::path const& path) const {
        return std::regex_match(fwdSlashPath(path), _re);
    }
}

