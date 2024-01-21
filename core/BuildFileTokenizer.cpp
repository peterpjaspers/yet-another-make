#pragma once

#include "BuildFileTokenizer.h"
#include <sstream>
#include <regex>

namespace
{
    using namespace YAM;

    struct EosTokenSpec : public ITokenSpec
    {
        bool match(const char* str, Token& token) const override { return false; }
        std::string const& type() const override { 
            static std::string eos("eos");
            return eos;
        }
    };
    EosTokenSpec eosSpec;
    ITokenSpec const* eosPtr = &eosSpec;
}
namespace YAM {

    BuildFileTokenizer::BuildFileTokenizer(
            std::filesystem::path const& filePath,
            std::string const& content)
        : _filePath(filePath)
        , _content(content)
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

    ITokenSpec const* BuildFileTokenizer::eosTokenSpec() { return eosPtr; }

    bool BuildFileTokenizer::eos() const {
        return _cursor >= _content.length();
    }

    Token BuildFileTokenizer::readNextToken(std::vector<ITokenSpec const*> const& specs) {
        Token token;
        token.spec = nullptr;
        if (eos()) {
            token.spec = eosTokenSpec();
            token.type = token.spec->type();
            return token;
        }
        auto cstr = _content.c_str() + _cursor;
        for (auto const& spec : specs) {
            if (spec->match(cstr, token)) {
                captureLocation(token.consumed);
                if (token.skip) token = readNextToken(specs);
                return token;
            }
        }
        return token;
    }

    void BuildFileTokenizer::captureLocation(std::size_t consumed) {
        // Absolute offsets.
        _tokenStartOffset = _cursor;

        // Line-based locations, start.
        _tokenStartLine = _line;
        _tokenStartColumn = _tokenStartOffset - _lineBeginOffset;

        // Extract `\n` in the consumes characters.
        auto cstr = _content.c_str() + _cursor;
        for (std::size_t i = 0; i < consumed; ++i) {
            if (cstr[i] == '\n') {
                _line++;
                _lineBeginOffset = _tokenStartOffset + i + 1;
            }
        }

        _tokenEndOffset = _cursor + consumed;

        // Line-based locations, end.
        _tokenEndLine = _line;
        _tokenEndColumn = _tokenEndOffset - _lineBeginOffset;
        _column = _tokenEndColumn;

        _cursor += consumed;
    }
}
