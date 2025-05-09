#ifndef LANG_READER_H
#define LANG_READER_H

#include "Types.h"
#include "Tokens.h"

#include <string>
#include <filesystem>
#include <iostream>

namespace Language {

    // Read program source code and convert to stream of tokens.

    struct Reader {
        // Character stream on program source code
        std::istream* stream;
        std::string fileName;
        // Current source line being read, source is read a line at a time
        std::string line;
        // Current source line number and character position (for error reporting)
        int lineNumber;
        size_t characterPosition;
        char character;
        Token token;
        // Parsed values associated with current token
        std::string identifier;
        VariableType identifierType;
        std::string stringConstant;
        int32_t integerConstant;
        float realConstant;
        // Read next character
        void nextCharacter();
        // Read next line
        void nextLine();
        // Read next token
        void nextToken();
        // Skip to requested Token
        void skipToToken( const Token to);
        // SKip white-space up to next token
        void skipWhiteSpace();
        Reader() = delete;
        // Construct a program Reader on a source-file.
        Reader( const std::filesystem::path& source );
        // Construct a program Reader on a source-string.
        Reader( const std::string& source );
        ~Reader();
    };

}

#endif // LANG_READER_H
