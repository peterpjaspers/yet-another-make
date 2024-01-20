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
        std::stringstream ss;
        ss
            << "Unexpected token at line " << _line
            << ", column " << _column << " in file " << _filePath.string()
            << std::endl;
        throw std::runtime_error(ss.str());
    }

    Token BuildFileTokenizer::readUntil(ITokenSpec const* spec, std::string& consumed) {
        Token endToken;
        while (!eos() && !spec->match(_content.c_str() + _cursor, endToken)) {
            consumed.push_back(readChar());
        }
        if (eos()) endToken.spec = eosTokenSpec();
        return endToken;
    }

    // Read character at current position.
    // If eos(): throw std::runtime_error.
    char BuildFileTokenizer::readChar() {
        if (eos()) throw std::runtime_error("Attempt to read beyond eos");
        auto cstr = _content.c_str() + _cursor;
        char c = cstr[0];
        captureLocation(c);
        _cursor += 1;
        return c;
    }

    void BuildFileTokenizer::captureLocation(std::string const& matched) {
        static std::regex nlRe(R"(\n)");

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

        _cursor += matched.length();
    }

    void BuildFileTokenizer::captureLocation(char c) {
        _tokenStartOffset = _cursor;
        _tokenStartLine = _line;
        _tokenStartColumn = _tokenStartOffset - _lineBeginOffset;

        if (c == '\\n') {
            _line++;
            _lineBeginOffset = 0;
        }

        _tokenEndOffset = _cursor + 1;

        _tokenEndLine = _line;
        _tokenEndColumn = _tokenEndOffset - _lineBeginOffset;
        _column = _tokenEndColumn;

        _cursor += 1;
    }
}
