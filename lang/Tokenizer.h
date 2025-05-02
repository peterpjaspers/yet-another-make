#ifndef LANG_TOKENIZER_H
#define LANG_TOKENIZER_H

#include "Types.h"

#include <string>
#include <filesystem>
#include <fstream>

namespace Language {

    enum Token {
        Undefined,
        EndOfFile,
        Identifier,
        IntegerConstant,
        RealConstant,
        StringConstant,
        Question,               // ?
        Colon,                  // :
        Exclamation,            // !
        Comma,                  // ,
        SemiColon,              // ;
        Period,                 // .
        LeftParen,              // (
        RightParen,             // )
        LeftSquare,             // [
        RightSquare,            // ]
        LeftCurly,              // {
        RightCurly,             // }
        SingleQuote,            // '
        DoubleQuote,            // "
        Apostrophe,             // `
        Tilde,                  // ~
        Plus,                   // +
        Minus,                  // -
        Asterisk,               // *
        Ampersand,              // &
        AmpersandAmpersand,     // &&
        Slash,                  // /
        BackSlash,
        VerticalBar,            // |
        VerticalVerticalBar,    // ||
        VerticalBarEqual,       // |=
        LeftAngle,              // <
        RightAngle,             // >
        LeftAngleEqual,         // <=
        RightAngleEqual,        // >=
        LeftLeftAngle,          // <<
        RightRightAngle,        // >>
        LeftLeftAngleEqual,     // <<=
        RightRightAngleEqual,   // >>=
        Equal,                  // =
        EqualEqual,             // ==
        Caret,                  // ^
        Percentile,             // %
        Dollar,                 // $
        AtSign,                 // @
        ExclamationEqual,       // !=
        AsteriskEqualSign,      // *=
        SlashEqual,             // /=
        PercentileEqual,        // %=
        PlusEqual,              // +=
        MinusEqual,             // -=
        AnpersandEqual,         // &=
        CaretEqual,             // ^=
        KeywordIf,
        KeywordThen,
        KeywordElse,
        KeywordWhile,
        KeywordReturn,
        KeywordDef
    };

    Token nextToken();
    void skipToToken( const Token to);

    // Convert program source in a series of tokens
    void tokenize( std::string program );
    void tokenize( std::filesystem::path program );

}

#endif // LANG_TOKENIZER_H
