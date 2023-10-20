#pragma once

#include "Tokenizer.h"
#include <sstream>


namespace YAM {
    Tokenizer::Tokenizer(std::string const& content, std::vector<TokenSpec> const& specs)
        : _content(content)
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

    void Tokenizer::readNextToken(Token& token) {
        token.type = "";
        token.value = "";

        if (!hasMoreTokens()) {
            token.type = "eos";
            return;
        }
        auto cstr = _content.c_str() + _cursor;
        for (auto const& spec : _specs) {
            if (match(spec.regex, cstr, token.value)) {
                token.type = spec.type;
                if (token.type == "skip") {
                    readNextToken(token);
                }
                return;
            }
        }
        throwUnexpectedToken();
    }

    bool Tokenizer::hasMoreTokens() {
        return _cursor < _content.length();
    }

    bool Tokenizer::match(std::regex const& re, const char* str, std::string& match) {
        std::cmatch cm;
        bool matched = std::regex_search(str, cm, re) && cm.position(0) == 0;
        if (!matched) return false;
        match = cm[0].str();
        captureLocation(match);
        _cursor += match.length();
        return true;
    }

    void Tokenizer::captureLocation(std::string const& matched) {
        std::regex nlRe(R"(\n)");

        // Absolute offsets.
        _tokenStartOffset = _cursor;

        // Line-based locations, start.
        _tokenStartLine = _line;
        _tokenStartColumn = _tokenStartOffset - _lineBeginOffset;

        // Extract `\n` in the matched token.
        std::size_t pos = 0;
        std::cmatch nlMatch;
        while (pos < matched.length() && std::regex_search(matched.c_str()+pos, nlMatch, nlRe)) {
            pos += nlMatch.position() + 1;
            _line++;
            _lineBeginOffset =
                _tokenStartOffset
                + static_cast<std::size_t>(nlMatch.position())
                + 1;
        }

        _tokenEndOffset = _cursor + matched.length();

        // Line-based locations, end.
        _tokenEndLine = _line;
        _tokenEndColumn = _tokenEndOffset - _lineBeginOffset;
        _column = _tokenEndColumn;
    }

    void Tokenizer::throwUnexpectedToken() {
    }
}
