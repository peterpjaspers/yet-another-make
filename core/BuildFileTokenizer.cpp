#pragma once

#include "BuildFileTokenizer.h"
#include <sstream>


namespace YAM {
    BuildFileTokenizer::BuildFileTokenizer(
        std::filesystem::path const& filePath, 
        std::string const& content, 
        std::vector<TokenSpec> const& specs)
        : _filePath(filePath)
        , _content(content)
        , _specs(specs)
        , _tokenStartOffset(0)
        , _tokenEndOffset(0)
        , _tokenStartLine(0)
        , _tokenEndLine(0)
        , _tokenStartColumn(0)
        , _tokenEndColumn(0)
        , _cursor(0)
        , _lineBeginOffset(0)
        , _line(0)
        , _column(0)
    {}

    void BuildFileTokenizer::readNextToken(Token& token) {
        token.type = "";
        token.value = "";

        if (!hasMoreTokens()) {
            token.type = "eos";
            return;
        }
        auto cstr = _content.c_str() + _cursor;
        for (auto const& spec : _specs) {
            if (match(spec, cstr, token.value)) {
                token.type = spec.type;
                if (token.type == "skip") {
                    readNextToken(token);
                }
                return;
            }
        }
        throwUnexpectedToken();
    }

    bool BuildFileTokenizer::hasMoreTokens() {
        return _cursor < _content.length();
    }

    bool BuildFileTokenizer::match(TokenSpec const& spec, const char* str, std::string& match) {
        std::cmatch cm;
        bool matched = std::regex_search(str, cm, spec.regex, std::regex_constants::match_continuous);
        if (!matched) return false;
        std::string m0 = cm[0].str();
        match = cm[spec.group].str();
        captureLocation(m0);
        _cursor += m0.length();
        return true;
    }

    void BuildFileTokenizer::captureLocation(std::string const& matched) {
        std::regex nlRe(R"(\n)");

        // Absolute offsets.
        _tokenStartOffset = _cursor;

        // Line-based locations, start.
        _tokenStartLine = _line;
        _tokenStartColumn = _tokenStartOffset - _lineBeginOffset;

        // Extract `\n` in the matched token.
        auto begin = std::sregex_iterator(matched.begin(), matched.end(), nlRe);
        auto end = std::sregex_iterator();
        for (std::sregex_iterator i = begin; i != end; ++i) {
            _line++;
            _lineBeginOffset = _tokenStartOffset + (*i).position() + 1;
        }

        _tokenEndOffset = _cursor + matched.length();

        // Line-based locations, end.
        _tokenEndLine = _line;
        _tokenEndColumn = _tokenEndOffset - _lineBeginOffset;
        _column = _tokenEndColumn;
    }

    void BuildFileTokenizer::throwUnexpectedToken() {
        std::stringstream ss;
        ss
            << "Unexpected token at line " << _line
            << ", column " << _column << " in file " << _filePath.string()
            << std::endl;
        throw std::runtime_error(ss.str());
    }
}
