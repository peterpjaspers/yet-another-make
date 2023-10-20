#pragma once

#include <regex>

namespace YAM {
    struct __declspec(dllexport) Token {
        Token() {}
        std::string type;
        std::string value;
    };

    struct __declspec(dllexport) TokenSpec {
        TokenSpec(std::string const& pattern, std::string const& tokenType)
            : regex(pattern)
            , type(tokenType)
        {}
        std::regex regex;
        std::string type;
    };

    class __declspec(dllexport) Tokenizer {
    public:
        Tokenizer(std::string const& content, std::vector<TokenSpec> const& specs);
        void readNextToken(Token& token);

        std::size_t tokenStartOffset() const { return _tokenStartOffset; }
        std::size_t tokenEndOffset() const { return _tokenEndOffset; }
        std::size_t tokenStartLine() const { return _tokenStartLine; }
        std::size_t tokenEndLine() const { return _tokenEndLine; }
        std::size_t tokenStartColumn() const { return _tokenStartColumn; }
        std::size_t tokenEndColumn() const{ return _tokenEndColumn; }

        std::size_t cursor() const { return _cursor; }
        std::size_t lineBeginOffset() const { return _lineBeginOffset; }
        std::size_t line() const { return _line; }
        std::size_t column() const { return _column; }

    private:
        bool match(std::regex const& re, const char* str, std::string& match);
        bool hasMoreTokens();
        void captureLocation(std::string const& matched);
        void throwUnexpectedToken();

        std::string const& _content; 
        std::vector<TokenSpec> const& _specs;

        Token _token;
        std::size_t _tokenStartOffset;
        std::size_t _tokenEndOffset;
        std::size_t _tokenStartLine;
        std::size_t _tokenEndLine;
        std::size_t _tokenStartColumn;
        std::size_t _tokenEndColumn;

        std::size_t _cursor;
        std::size_t _lineBeginOffset;
        std::size_t _line;
        std::size_t _column;
    };
}
