#include "Translator.h"

#include "SymbolTable.h"
#include "Instruction.h"
#include "Monitor.h"
#include "Memory.h"

#include <iostream>
#include <sstream>
#include <map>

// ToDo: Name-scoping via block statements and procedures (nested symbol table and re-use of local storage)
// ToDo: Named arguments (requires name-scoping)
// ToDo: Thread-safe translation and execution
// ToDo: Python like format strings
// ToDo: Better error handling; i.e., try to recover
// ToDo: Include file-name (if any) in error message

using namespace std;
using namespace std::filesystem;

namespace Language {

    namespace {

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
            KeywordElse,
            KeywordWhile,
            KeywordReturn,
            KeywordDef,
            KeywordOut,
            KeywordImport
        };

        const map<string,Token> keywords = {
            { string( "if" ), KeywordIf },
            { string( "else" ), KeywordElse },
            { string( "while" ), KeywordWhile },
            { string( "return" ), KeywordReturn },
            { string( "def" ), KeywordDef },
            { string( "out" ), KeywordOut },
            { string( "import" ), KeywordImport },
        };

        enum IdentifierType {
            None,
            Local,
            Argument,
            Global,
            Scoped
        };

        void nextCharacter();
        void skipWhiteSpace();
        void nextToken();
        void skipToToken( const Token to );

        struct Translator {
            // Character stream on program source code
            istream* stream;
            // Current source line being parsed, source is read a line at a time
            string line;
            // Current source line number and character position (for error reporting)
            int lineNumber;
            size_t characterPosition;
            char character;
            Token token;
            Translator( istream* source = nullptr ) :
                stream( source ),
                line( "" ),
                lineNumber( 0 ),
                characterPosition( 0 ),
                character( ' ' ),
                token( Undefined )
            {}
        };
        vector<Translator> parseStates;
        Translator state;
        // Parsed values associated with current token
        string identifier;
        IdentifierType identifierType( IdentifierType::None );
        string stringConstant;
        int32_t integerConstant;
        float realConstant;
        // Current extent of local variable descriptors
        Word locals( 0 );
        // Flag indicating that expression value has not been consumed (by a semi-colon)
        bool unconsumed( false );
        // Error count, increased on each recoverable error
        int errors( 0 );

        void fatalError( const char* message ) { errors += 1; throw string( "Parse error : " ) + message; }
        void recoverableError( const char* message, Token to ) {
            cerr << "Parse error line " << state.lineNumber << ", column " << state.characterPosition << " : " << message << endl;
            errors += 1;
            skipToToken( to );
        }

        // Control flow jump address administration. Forward jump addresses are patched
        // in generated program when the actual target address is defined.
        typedef uint32_t PatchLabel;
        PatchLabel nextPatch( 0 );
        map<PatchLabel,Address> patches;
        // Create a patch entry for an instruction at the current program address,
        // returning a label to perform the actual patch.
        PatchLabel createPatch() {
            if (nextPatch == MAXINT32) fatalError( "Out of jump labels" );
            auto label( nextPatch++ );
            patches.insert( { label, allocateProgram( 0 ) } );
            return label;
        }
        // Patch (set) the operand value of the (previously generated) instruction associated with a patch label
        void patchOperand( const PatchLabel label, const Word operand ) {
            auto entry( patches.find( label ) );
            if (entry == patches.end()) fatalError( "Internal error, invalid patch label" );
            auto opAddress( entry->second );
            auto operandAddress( reinterpret_cast<Word*>( addressProgram( opAddress ) + sizeof( OpCode ) ) );
            *operandAddress = operand;
        }
        // Set target address of the jump instruction associated with a patch label to the current program address.
        void patchJump( const PatchLabel label ) { patchOperand( label, allocateProgram( 0 ) ); }
        // Read (next) line from program source
        void nextLine() {
            getline( *state.stream, state.line );
            state.lineNumber += 1;
            if (monitor( DebugAspects::SourceCode )) monitorRecord() << setw( 6 ) << state.lineNumber << " : " << state.line << record<char>;
            state.characterPosition = 0;            
        }
        // Read next character from source, reading a new line from the source as required 
        void nextCharacter() {
            while (state.line.size() <= state.characterPosition) {
                if (state.stream && state.stream->eof()) { state.character = EOF; break; }
                else if (state.stream && state.stream->good()) nextLine();
                else fatalError( "Source read error" );
            }
            if (state.character != EOF) state.character = state.line[ state.characterPosition++  ];
        }
        void skipWhiteSpace() { while (isspace( static_cast<unsigned char>( state.character ) ) && (state.character != EOF)) nextCharacter(); }
        inline bool isIdentifier( char c ) { return( isalpha( c ) || (c == '_') ); }
        // ToDo: Table driven tokenizer to avoid large if then else construct
        void nextToken() {
            skipWhiteSpace();
            if (isIdentifier( state.character )) {
                identifier.clear();
                identifier += state.character;
                nextCharacter();
                while (isIdentifier( state.character )) { identifier += state.character; nextCharacter(); }
                auto keyword( keywords.find( identifier ) );
                if (keyword == keywords.end()) {
                    state.token = Identifier;
                    identifierType = IdentifierType::Local;
                } else state.token = keyword->second;
            } else if (state.character == ':') {
                nextCharacter();
                if (isIdentifier( state.character )) {
                    while (isIdentifier( state.character )) { identifier += state.character; nextCharacter(); }
                    state.token = Identifier;
                    identifierType = IdentifierType::Global;
                } else {
                    state.token = Dollar;
                }
            } else if (state.character == '$') {
                nextCharacter();
                if (isdigit( state.character )) {
                    string number;
                    while (isdigit( state.character )) { identifier += state.character; number += state.character; nextCharacter(); }
                    integerConstant = stoi( number );
                    state.token = Identifier;
                    identifierType = IdentifierType::Argument;
                } else {
                    state.token = Dollar;
                }
            } else if (isdigit( state.character )) {
                // Parse integer or real
                string number;
                while (isdigit( state.character )) { number += state.character; nextCharacter(); }
                if (state.character == '.') {
                    // Parsing real
                    number += state.character; nextCharacter();
                    while (isdigit( state.character )) { number += state.character; nextCharacter(); }
                    realConstant = stof( number );
                    state.token = RealConstant;
                } else {
                    integerConstant = stoi( number );
                    state.token = IntegerConstant;
                }
            } else if (state.character == '\"' ) {
                // Parse string constant
                stringConstant.clear();
                nextCharacter();
                while (state.character != '\"') {
                    if (state.character == '\\') { nextCharacter(); }
                    stringConstant += state.character;
                    nextCharacter();
                }
                nextCharacter();
                state.token = StringConstant;
            } else if (state.character == '?' ) { state.token = Question; nextCharacter(); }
            else if (state.character == ':' ) { state.token = Colon; nextCharacter(); }
            else if (state.character == ',' ) { state.token = Comma; nextCharacter(); }
            else if (state.character == ';' ) { state.token = SemiColon; nextCharacter(); }
            else if (state.character == '.' ) { state.token = Period; nextCharacter(); }
            else if (state.character == '(' ) { state.token = LeftParen; nextCharacter(); }
            else if (state.character == ')' ) { state.token = RightParen; nextCharacter(); }
            else if (state.character == '[' ) { state.token = LeftSquare; nextCharacter(); }
            else if (state.character == ']' ) { state.token = RightSquare; nextCharacter(); }
            else if (state.character == '{' ) { state.token = LeftCurly; nextCharacter(); }
            else if (state.character == '}' ) { state.token = RightCurly; nextCharacter(); }
            else if (state.character == '\'' ) { state.token = SingleQuote; nextCharacter(); }
            else if (state.character == '"' ) { state.token = DoubleQuote; nextCharacter(); }
            else if (state.character == '`' ) { state.token = Apostrophe; nextCharacter(); }
            else if (state.character == '<' ) {
                nextCharacter();
                if (state.character == '<' ) {
                    if (state.character == '=' ) { state.token = LeftLeftAngleEqual; nextCharacter(); }
                    else { state.token = LeftLeftAngle; nextCharacter(); }
                } else if (state.character == '=' ) { state.token = LeftAngleEqual; nextCharacter(); }
                else state.token = LeftAngle;
            } else if (state.character == '>' ) {
                nextCharacter();
                if (state.character == '>' ) {
                    if (state.character == '=' ) { state.token = RightRightAngleEqual; nextCharacter(); }
                    else { state.token = RightRightAngle; nextCharacter(); }
                } else if (state.character == '=' ) { state.token = RightAngleEqual; nextCharacter(); }
                else state.token = RightAngle;
            } else if (state.character == '!') {
                nextCharacter();
                if (state.character == '=' ) { state.token = ExclamationEqual; nextCharacter(); }
                else state.token = Exclamation;
            } else if (state.character == '+') {
                nextCharacter();
                if (state.character == '=' ) { state.token = PlusEqual; nextCharacter(); }
                else state.token = Plus;
            } else if (state.character == '-') {
                nextCharacter();
                if (state.character == '=' ) { state.token = MinusEqual; nextCharacter(); }
                else state.token = Minus;
            } else if (state.character == '*') {
                nextCharacter();
                if (state.character == '=' ) { state.token = AsteriskEqualSign; nextCharacter(); }
                else state.token = Asterisk;
            } else if (state.character == '!') {
                nextCharacter();
                if (state.character == '=' ) { state.token = ExclamationEqual; nextCharacter(); }
                else state.token = Exclamation;
            } else if (state.character == '&') {
                nextCharacter();
                if (state.character == '&' ) { state.token = AmpersandAmpersand; nextCharacter(); }
                if (state.character == '=' ) { state.token = AnpersandEqual; nextCharacter(); }
                else state.token = Ampersand;
            } else if (state.character == '/') {
                nextCharacter();
                if (state.character == '=' ) { state.token = SlashEqual; nextCharacter(); }
                // ToDo: avoid recursive call to state.token when consuming comments
                else if (state.character == '/') { nextLine(); nextCharacter(); nextToken(); }
                else state.token = Slash;
            } else if (state.character == '|') {
                nextCharacter();
                if (state.character == '|' ) { state.token = VerticalVerticalBar; nextCharacter(); }
                else if (state.character == '=' ) { state.token = VerticalBarEqual; nextCharacter(); }
                else state.token = VerticalBar;
            } else if (state.character == '=') {
                nextCharacter();
                if (state.character == '=' ) { state.token = EqualEqual; nextCharacter(); }
                else state.token = Equal;
            } else if (state.character == '^') {
                nextCharacter();
                if (state.character == '=' ) { state.token = CaretEqual; nextCharacter(); }
                else state.token = Caret;
            } else if (state.character == '%') {
                nextCharacter();
                if (state.character == '=' ) { state.token = PercentileEqual; nextCharacter(); }
                else state.token = Percentile;
            } else if (state.character == '~') {
                nextCharacter();
                state.token = Tilde;
            } else if (state.character == '@') {
                nextCharacter();
                state.token = AtSign;
            } else if (state.character == '\\') {
                nextCharacter();
                state.token = BackSlash;
            } else if (state.character == EOF) {
                state.token = EndOfFile;
            } else {
                fatalError( "Illegal state.character" );
            }
        };
        void skipToToken( const Token to ) { while ((state.token != to) && (state.token != EndOfFile)) nextToken(); }

        bool parseExpression();
        bool parseDyadicExpression();
        bool parseMonadicExpression();
        bool parsePostfixExpression();
        bool parsePrimaryExpression();

        // <expression> ::= <dyadic> | <assignment>
        // <assignment> :: <monadic> <assignment-op> <expression>
        // <assignment-op> ::= '=' | '*=' | '/='| '%=' | '+=' | '-=' | '<<=' | '>>=' | '&=' | '^=' | '|='
        // <dyadic> ::= <monadic> | <dyadic> <dyadic-op> <monadic>
        // <dyadic-op> ::=
        //        '||' | '&&' |                             // logical
        //         '|' | '^' | '&' |                        // bit-wise
        //        '==' | '!=' | '<' | '>' | '<=' | '>=' |   // relatation
        //        '<<' | '>>' |                             // shift
        //        '+' | '-' | '*' | '/'                     // arithmetic
        struct OpPrecedence { Token token; int precedence; OpCode code; };
        vector<OpPrecedence> operatorStack( { OpPrecedence{ Undefined, -2, 0 } } );
        void generateExpressionCode( const OpPrecedence op ) {
            auto top( operatorStack.back() );
            while (top.precedence >= op.precedence) { storeInstruction( top.code ); operatorStack.pop_back(); top = operatorStack.back(); }
            if (op.token != Undefined) operatorStack.push_back( op );
        }
        inline void generateExpressionCode() { generateExpressionCode( OpPrecedence{ Undefined, -1, 0 } ); }
        inline void suspendExpressionCode() { operatorStack.push_back( OpPrecedence{ Undefined, -2, 0 } ); }
        inline void resumeExpressionCode() { generateExpressionCode(); operatorStack.pop_back(); }
        bool parseDyadicExpression() {
            static const OpPrecedence ops[] = {
                { VerticalVerticalBar, 1, OpOr }, { AmpersandAmpersand, 2, OpAnd },
                { VerticalBar, 3, OpBitOr }, { Caret, 4, OpBitXor }, { Ampersand, 5, OpBitAnd },
                { EqualEqual, 6, OpEq }, { ExclamationEqual, 6, OpNeq },
                { LeftAngle, 7, OpLt }, { RightAngle, 7, OpGt }, { LeftAngleEqual, 7, OpLteq }, { RightAngleEqual, 7, OpGteq },
                { LeftLeftAngle, 8, OpShiftLeft }, { RightRightAngle, 8, OpShiftRight },
                { Plus, 9, OpAdd }, { Minus, 9, OpSub },
                { Asterisk, 10, OpMul }, { Slash, 10, OpDiv }, { Percentile, 10, OpRem }
            };
            if (monitor( DebugAspects::ParserFunctions )) monitorRecord() << setw( 20 ) << "" << "parseDyadicExpression" << record<char>;
            auto parsed( parseMonadicExpression() );
            if (parsed) {
                bool parseAssignment( true );
                for (int i = 0; i < (sizeof( ops ) / sizeof( OpPrecedence )); ++i) {
                    if ((ops[ i ].token) == state.token) {
                        nextToken();
                        if (ops[ i ].code != 0) generateExpressionCode( ops[ i ] );
                        parsed = parseDyadicExpression();
                        parseAssignment = false;
                        break;
                    }
                }
                if (parseAssignment) {
                    static const OpPrecedence asignOps[] = {
                        { AsteriskEqualSign, 10, OpMul }, { SlashEqual, 10, OpDiv }, { PercentileEqual, 10, OpRem },
                        { PlusEqual, 9, OpAdd }, { MinusEqual, 9, OpSub },
                        { LeftLeftAngleEqual, 8, OpShiftLeft }, { RightRightAngleEqual, 8, OpShiftRight },
                        { AnpersandEqual, 5, OpBitAnd }, { CaretEqual, 4, OpBitXor }, { VerticalBarEqual, 3, OpBitOr }
                    };
                    if (state.token == Equal) {
                        nextToken();
                        parsed = parseDyadicExpression();
                        if (parsed) generateExpressionCode( { Equal, 0, OpAssign } );
                    } else {
                        for (int i = 0; i < (sizeof( asignOps ) / sizeof( OpPrecedence )); ++i) {
                            if ((asignOps[ i ].token) == state.token) {
                                nextToken();
                                storeInstruction( OpDup );
                                parsed = parseDyadicExpression();
                                if (asignOps[ i ].code != 0) generateExpressionCode( asignOps[ i ] );
                                if (parsed) {
                                    generateExpressionCode( { Equal, 0, OpAssign } );
                                } else {
                                    recoverableError( "Expected expression", SemiColon );
                                }
                                break;
                            }
                        }
                    }
                }
            }
            return parsed;
        }
        // <monadic> ::= [ <monadic-op> ] <postfix>
        // <monadic-op> ::= '+' | '-' | '~' | '!'
        bool parseMonadicExpression() {
            static const OpPrecedence ops[] = {
                { Plus, 11, 0 }, { Minus, 11, OpNegate },   // arithmetic
                { Tilde, 11, OpInvert },                    // bit-wise
                { Exclamation, 11, OpNot }                  // logical
            };
            if (monitor( DebugAspects::ParserFunctions )) monitorRecord() << setw( 20 ) << "" << "parseMonadicExpression" << record<char>;
            for (int i = 0; i < (sizeof( ops ) / sizeof( Token )); ++i) {
                if (ops[ i ].token == state.token) {
                    nextToken();
                    auto parsed( parseMonadicExpression() );
                    if (ops[ i ].code != 0) generateExpressionCode( ops[ i ] );
                    return parsed;
                }
            }
            return parsePostfixExpression();
        }
        // <postfix> ::= <primary> [ <index> | <arguments> ]
        // <index> ::= '[' <expression> ']'
        // <arguments> ::= '(' <expression> [ ',' <expression> ]+ ')'
        bool parsePostfixExpression() {
            if (monitor( DebugAspects::ParserFunctions )) monitorRecord() << setw( 20 ) << "" << "parsePostfixExpression" << record<char>;
            auto parsed( parsePrimaryExpression() );
            if (parsed && (state.token == RightSquare)) {
                nextToken();
                parsed = parseExpression();
                if (parsed && state.token == LeftSquare) {
                    nextToken();
                    // ToDo: Generate code to index list, set or map
                }
                else { recoverableError( "Expected ]", SemiColon ); parsed = false; }
            } else if (state.token == LeftParen) {
                nextToken();
                // Procedure-call has highest priority,
                // suspend (lower-priority) expression evaluation during argument expression evaluation...
                suspendExpressionCode();
                storeInstruction( OpArguments );
                // Parse argument expressions
                while (parsed) {
                    parsed = parseExpression();
                    if (state.token != Comma) break;
                    nextToken();
                }
                if (state.token == RightParen) {
                    nextToken();
                    storeInstruction( OpProcedureCall );
                    // ... resume (lower-priority) expression evaluation
                    resumeExpressionCode();
                } else { recoverableError( "Expected )", SemiColon ); parsed = false; }
            }
            return parsed;
        }
        // <primary> ::= <identifier> | <integer> | <real> | <string> | '(' <expression> ')'
        // <identifier> ::= <local-identifier> | <global-identifier> | <argument-identifier> | <scoped-identifier>
        // <local-identifier> ::= <alpha> [ <alpha-numeric> | '_' ]*
        // <scoped-identifier> ::= ':' <local-identifier>
        // <argument-identifier> ::= '$' <integer>
        // <scoped-identifier> ::= <local-identifier> [ ':' <local-identifier> ]+
        // <integer> ::= <digit> [ <digit> ]*
        // <real> ::= <integer> '.' [ <integer> ]
        // <string> ::= '"' [ <character> ]* '"'
        bool parsePrimaryExpression() {
            if (monitor( DebugAspects::ParserFunctions )) monitorRecord() << setw( 20 ) << "" << "parsePrimaryExpression" << record<char>;
            if (state.token == Identifier) {
                auto type( symbolType( identifier ) );
                if (type == Undefined) {
                    if (identifierType == IdentifierType::Local) {
                        createLocalVariable( identifier, locals );
                        storeInstruction( OpPushDescriptor, LocalVariableDescriptor( locals ) );
                        locals += sizeof( Descriptor );
                    } else if (identifierType == IdentifierType::Argument) {
                        auto offset( integerConstant * sizeof( Descriptor ) );
                        createArgumentVariable( identifier, offset );
                        storeInstruction( OpPushDescriptor, ArgumentVariableDescriptor( offset ) );
                    } else if (identifierType == IdentifierType::Global) {
                        auto address( allocateHeap( sizeof(Descriptor) ) );
                        createGlobalVariable( identifier, address );
                        storeInstruction( OpPushDescriptor, GlobalVariableDescriptor( address ) );
                    } else fatalError( "Internal parser error - Invalid identifier type" );
                } else if (type == LocalVariable) {
                    auto offset( localVariableOffset( identifier ) );
                    storeInstruction( OpPushDescriptor, LocalVariableDescriptor( offset ) );
                } else if (type == ArgumentVariable) {
                    auto offset( argumentVariableAddress( identifier ) );
                    storeInstruction( OpPushDescriptor, ArgumentVariableDescriptor( offset ) );
                } else if (type == GlobalVariable) {
                    auto address( globalVariableAddress( identifier ) );
                    storeInstruction( OpPushDescriptor, GlobalVariableDescriptor( address ) );
                } else if (type == Procedure) {
                    auto address( procedureAddress( identifier ) );
                    storeInstruction( OpPushDescriptor, ProcedureDescriptor( address ) );
                }
                nextToken();
            } else if (state.token == IntegerConstant) {
                storeInstruction( OpPushInteger, (Word)integerConstant ); nextToken();
            } else if (state.token == RealConstant) {
                storeInstruction( OpPushReal, bit_cast<Word>( realConstant ) ); nextToken();
            } else if (state.token == StringConstant) {
                auto n( stringConstant.size() );
                auto address( allocateString( n ) );
                strncpy( (char*)addressString( address ), stringConstant.c_str(), n );
                storeInstruction( OpPushDescriptor, StringDescriptor( address, n ) );
                nextToken();
            } else if (state.token == LeftParen) {
                nextToken();
                auto parsed( parseExpression() );
                if (state.token == RightParen) nextToken();
                else { recoverableError( "Expected )", SemiColon ); parsed = false; }
                return parsed;
            } else {
                recoverableError( "Expected expression", SemiColon );
                return false;
            }
            return true;
        }
        bool parseExpression() {
            if (monitor( DebugAspects::ParserFunctions )) monitorRecord() << setw( 20 ) << "" << "parseExpression" << record<char>;
            auto parsed( parseDyadicExpression() );
            generateExpressionCode();
            return parsed;
        }

        bool parseSource( istream* source );
        bool parse();
        bool parseStatement();
        bool parseBlock();
        bool parseIfStatement();
        bool parseWhileStatement();
        bool parseReturnStatement();
        bool parseDefStatement();

        //  <block-statement> ::=
        //      '{' [ <statement> ]* '}'
        bool parseBlock() {
            if (state.token == LeftCurly) {
                nextToken();
                while ((state.token != RightCurly) && (state.token != EndOfFile)) parseStatement();
                if (state.token == RightCurly) { nextToken(); return true; }
                else recoverableError( "Expected }", SemiColon );
            }
            return false;
        }
        //  <if-statement> ::=
        //      'if' '(' <expression> ')' <statement> [ 'else' <statement> ]
        bool parseIfStatement() {
            if (state.token == KeywordIf) {
                nextToken();
                if (state.token == LeftParen) {
                    nextToken();
                    parseExpression();
                    auto elseLabel( createPatch() );
                    storeInstruction( OpConditionalJump, Address( 0 ) );
                    if (state.token == RightParen) {
                        nextToken();
                        parseStatement();
                        if (state.token == KeywordElse) {
                            nextToken();
                            auto endLabel( createPatch() );
                            storeInstruction( OpJump, Address( 0 ) );
                            patchJump( elseLabel );
                            parseStatement();
                            patchJump( endLabel );
                        } else {
                            patchJump( elseLabel );
                        }
                    } else {
                        recoverableError( "Expected )", SemiColon );
                    }
                } else {
                    recoverableError( "Expected (", SemiColon );
                }
            }
            return true;
        }
        //  <while-statement> ::=
        //      'while' '(' <expression> ')' <statement>
        bool parseWhileStatement() {
            if (state.token == KeywordWhile) {
                nextToken();
                if (state.token == LeftParen) {
                    nextToken();
                    auto loopLabel( allocateProgram( 0 ) );
                    parseExpression();
                    auto endLabel( createPatch() );
                    storeInstruction( OpConditionalJump, Address( 0 ) );
                    if (state.token == RightParen) {
                        nextToken();
                        parseStatement();
                        storeInstruction( OpJump, loopLabel );
                        patchJump( endLabel );
                    } else {
                        recoverableError( "Expected )", SemiColon );
                    }
                } else {
                    recoverableError( "Expected (", SemiColon );
                }
            }
            return true;
        }
        //  <output-statement> ::=
        //      'out' <expression> ';'
        bool parseOutStatement() {
            if (state.token == KeywordOut) {
                nextToken();
                parseExpression();
                if (state.token == SemiColon) {
                    nextToken();
                    storeInstruction( OpOutput );
                    return true;
                }
                else recoverableError( "Expected ;", SemiColon );
            }
            return false;
        }
        //  <return-statement> ::=
        //      'return' [ <expression> ] ';'
        bool parseReturnStatement() {
            if (state.token == KeywordReturn) {
                nextToken();
                if (state.token != SemiColon) {
                    parseExpression();
                } else {
                    // Procedure returns Null if no expression is provided
                    storeInstruction( OpPushNull );
                }
                if (state.token == SemiColon) {
                    nextToken();
                    storeInstruction( OpReturn );
                    return true;
                }
                else recoverableError( "Expected ;", SemiColon );
            }
            return false;
        }
        //  <procedure-definition> ::=
        //      'def' <local-identifier> <statement>
        // Procedures have no formal argument list, instead all procedures have a variable number of
        // arguments indexed by $0, $1, .. $N where N is the actual number or argumnts provided
        // ToDo: provide means of retrieving actual number of arguments via intrinsics (e.g., args() and argv(i) )
        bool parseDefStatement() {
            if (state.token == KeywordDef) {
                nextToken();
                if (state.token == Identifier) {
                    nextToken();
                    // Provisionally Insert code to jump over procedure definition
                    // ToDo: Jump can be avoided by determining start address of code for a translation unit.
                    auto skip( createPatch() );
                    storeInstruction( OpJump, Address( 0 ) );
                    createProcedure( identifier, allocateProgram( 0 ) );
                    auto previousLocals( locals );
                    auto opLocals( createPatch() );
                    storeInstruction( OpLocals, Word( 0 ) );
                    if (state.token == LeftCurly) parseBlock();
                    else recoverableError( "Expected {", SemiColon );
                    // ToDo: Only generate return if required (might not be worth the trouble)
                    storeInstruction( OpReturn );
                    patchOperand( opLocals, locals );
                    locals = previousLocals;
                    patchJump( skip );
                } else recoverableError( "Expected identifier", SemiColon );
            }
            return true;
        }
        // <import-statement> ::=
        //      'import' '<' <file-name> '>'
        bool parseImport() {
            if (state.token == KeywordImport) {
                nextToken();
                if (state.token == LeftAngle) {
                    string fileName( &state.character, 1 );
                    nextCharacter();
                    while ((state.character != '>') && (state.character != '/n') && (state.character != EOF)) {
                        fileName += state.character; nextCharacter();
                    }
                    if (state.character == '>') {
                        auto stream( new ifstream( fileName ) );
                        if (stream->good()) return parseSource( stream );
                        else recoverableError( "Could not open import file", SemiColon );
                    } else recoverableError( "Missing > in import", SemiColon );
                }
            }
            return false;
        }
        // <statement> ::=
        //      <expression> ';' |
        //      <block-statement> |
        //      <if-statement> |
        //      <while-statement> |
        //      <output-statement> |
        //      <return-statement> |
        //      <procedure-definition> |
        //      <import-statement>
        bool parseStatement() {
            if (state.token == LeftCurly) return parseBlock();
            else if (state.token == KeywordIf) return parseIfStatement();
            else if (state.token == KeywordWhile) return parseWhileStatement();
            else if (state.token == KeywordDef) return parseDefStatement();
            else if (state.token == KeywordOut) return parseOutStatement();
            else if (state.token == KeywordReturn) return parseReturnStatement();
            else if (state.token == KeywordImport) return parseImport();
            else {
                if (unconsumed) {
                    storeInstruction( OpPop ); // Consume value of last evaluated expression
                    unconsumed = false;
                }
                if (state.token != SemiColon) parseExpression();
                if (state.token == SemiColon) {
                    nextToken();
                    unconsumed = true;
                    return true;
                }
                recoverableError( "Expected ;", SemiColon );
            }
            return false;
        }
        // <program> ::= [ <statement> ]*
        bool parse() {
            if (monitor( DebugAspects::ParserFunctions )) monitorRecord() << setw( 20 ) << "" << "parse" << record<char>;
            bool parsed( true );
            nextCharacter();
            nextToken();
            while (state.token != EndOfFile) parsed = parsed && parseStatement();
            return parsed;
        }
        
        bool parseSource( istream* stream ) {
            if (monitor( DebugAspects::ParserFunctions )) monitorRecord() << setw( 20 ) << "" << "parseSource" << record<char>;
            if (state.stream != nullptr) parseStates.push_back( state );
            state = Translator( stream );
            auto parsed( parse() );
            delete state.stream;
            state.stream = nullptr;
            if (0 < parseStates.size()) {
                state = parseStates.back();
                parseStates.pop_back();
                if (parsed) parsed = parse();
            }
            return parsed;
        }

    } // namespace unnamed

    Address translate( istream* stream ) {
        // ToDo: Start address may not be current program address as code may start with
        // procedure definitions...
        auto start( allocateProgram( 0 ) );
        auto usage( currentMemoryUsage() );
        bool parsed( true );
        try {
            auto opLocals( createPatch() );
            storeInstruction( OpLocals, Word( 0 ) );
            parseSource( stream );
            patchOperand( opLocals, locals );
            storeInstruction( OpExit );
            return( start );
        } catch(...) { return false; }
        if (errors != 0) {
            // Parsing failed, reclaim provisionally allocated memory
            recoverMemory( usage );
            // Replace failed translation unit with (procedure) code returning null
            storeInstruction( OpLocals, Word(0) );
            storeInstruction( OpPushNull );
            storeInstruction( OpReturn );
        };
        return start;
    }
    Address translate( string program ) { return translate( new istringstream( program ) ); }
    Address translate( path program ) { return translate( new ifstream( program ) ); }

} // namespace Language

