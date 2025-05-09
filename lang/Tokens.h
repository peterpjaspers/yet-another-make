#ifndef LANG_TOKENS_H
#define LANG_TOKENS_H

namespace Language {

    enum Token {
        None,
        EndOfFile,
        Illegal,
        ReadError,
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
        KeywordVar,
        KeywordIf,
        KeywordThen,
        KeywordElse,
        KeywordWhile,
        KeywordDef,
        KeywordReturn,
        KeywordOut,
        KeywordImport,
        KeywordInclude,
    };

}

#endif // LANG_TOKENS_H
