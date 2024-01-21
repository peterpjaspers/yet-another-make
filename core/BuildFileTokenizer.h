#pragma once

#include "Token.h"

#include <filesystem>
#include <memory>

namespace YAM {
    class __declspec(dllexport) BuildFileTokenizer {
    public:
        BuildFileTokenizer(
            std::filesystem::path const& filePath,
            std::string const& content);

        // Return the spec that matches eos.
        static ITokenSpec const* eosTokenSpec();

        std::filesystem::path const& filePath() const { return _filePath; }
        std::string const& content() const { return _content; }

        // Return whether end-of-stream has been reached;
        bool eos() const;

        // Read token at current position that matches one of the given
        // token specifications.
        // If eos(): returned token.spec == eosTokenSpec and token.type == "eos".
        // If no match could be found: returned token.spec == nullptr.
        Token readNextToken(std::vector<ITokenSpec const*> const& specs);

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
        void captureLocation(std::size_t consumed);

        std::filesystem::path const& _filePath;
        std::string const& _content; 

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
