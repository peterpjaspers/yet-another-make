#include "Translator.h"

#include "Reader.h"
#include "SymbolTable.h"
#include "Instruction.h"
#include "Monitor.h"
#include "ThreadContext.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <set>

// ToDo: Thread-safe translation and execution
// ToDo: Python like format strings
// ToDo: Better error handling; i.e., try to recover
// ToDo: Implement multiple instances of interpreter (Instruction.cpp) and memory pools (Memory.cpp)

using namespace std;
using namespace std::filesystem;

namespace Language {

    namespace {

        struct OpPrecedence {
            Token token;
            int precedence;
            OpCode code;
            OpPrecedence( const Token t, const int p, const OpCode c ) : token( t ), precedence( p ), code( c ) {}
        };

        class TranslateState {
            Reader* reader;
            vector<Reader*> pausedReaders;
            // Current extent of local variable descriptors
            Word locals;
            Word maxLocals;
            // Flag indicating that expression value has been consumed
            // by a semi-colon or an instruction operating on the instruction.
            bool consumed;
            // Error count, increased on each recoverable error
            int errors;
            // Control flow jump address administration. Forward jump addresses are patched
            // in generated program when the actual target address is defined.
            typedef uint32_t PatchLabel;
            PatchLabel nextPatch;
            map<PatchLabel,Address> patches;
            vector<OpPrecedence> operatorStack;
            // Scope is a list of names corresponding to the nested scope currently being translated.
            // The list holds namespace names, procedure names and annonymous block indeces.
            // The scope list is used to uniquely identify variable names.
            vector<string> scopeNames;
            vector<int> blockIndeces;
            // Maintain a set of imported file names.
            // A file is only imported once depending on its presence in the set.
            set<string> importFileNames;

            inline Token token() { return reader->token; }
            inline char character() { return reader->character; }
            inline void nextCharacter() { return reader->nextCharacter(); }
            inline void nextToken() { return reader->nextToken(); }
            inline void skipToToken( Token token ) { return reader->skipToToken( token ); }
            inline const string& identifier() { return reader->identifier; }

            inline void nextLocal() {
                locals += sizeof( Descriptor );
                if (maxLocals < locals) maxLocals = locals;
            }

            void fatalError( const char* message ) { errors += 1; throw string( "Parse error : " ) + message; }
            void recoverableError( const char* message, Token to ) {
                cerr << reader->fileName << " : Parse error line " << reader->lineNumber << ", column " << reader->characterPosition << " : " << message << endl;
                errors += 1;
                skipToToken( to );
            }

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

            void enterNamedScope( const string name ) {
                scopeNames.push_back( name );
                blockIndeces.push_back( 0 );
            }
            void enterAnonymousScope() {
                static const char* signature( "void enterAnonymousScope()" );
                if (blockIndeces.size() == 0) throw string( signature ) + "Internal parser error, no scope defined";
                auto index( blockIndeces.back() );
                blockIndeces.pop_back();
                blockIndeces.push_back( index + 1 );
                scopeNames.push_back( to_string( index ) );
                blockIndeces.push_back( 0 );
            }
            void exitScope() {
                static const char* signature( "void exitScope()" );
                if (blockIndeces.size() == 0) throw string( signature ) + "Internal parser error, no scope defined";
                scopeNames.pop_back();
                blockIndeces.pop_back();
            }

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
            void generateExpressionCode( const OpPrecedence op ) {
                auto top( operatorStack.back() );
                while (top.precedence >= op.precedence) { storeInstruction( top.code ); operatorStack.pop_back(); top = operatorStack.back(); }
                if (op.token != Token::None) operatorStack.push_back( op );
            }
            inline void generateExpressionCode() { generateExpressionCode( OpPrecedence{ Token::None, -1, 0 } ); }
            inline void suspendExpressionCode() { operatorStack.push_back( OpPrecedence{ Token::None, -2, 0 } ); }
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
                        if ((ops[ i ].token) == token()) {
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
                        if (token() == Token::Equal) {
                            nextToken();
                            parsed = parseDyadicExpression();
                            if (parsed) generateExpressionCode( { Equal, 0, OpAssign } );
                        } else {
                            for (int i = 0; i < (sizeof( asignOps ) / sizeof( OpPrecedence )); ++i) {
                                if ((asignOps[ i ].token) == token()) {
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
                    if (ops[ i ].token == token()) {
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
                if (parsed && (token() == Token::RightSquare)) {
                    nextToken();
                    parsed = parseExpression();
                    if (parsed && token() == Token::LeftSquare) {
                        nextToken();
                        // ToDo: Generate code to index list, set or map
                    }
                    else { recoverableError( "Expected ]", SemiColon ); parsed = false; }
                } else if (token() == Token::LeftParen) {
                    nextToken();
                    // Procedure-call has highest priority,
                    // suspend (lower-priority) expression evaluation during argument expression evaluation...
                    suspendExpressionCode();
                    storeInstruction( OpArguments );
                    // Parse argument expressions
                    while (parsed) {
                        parsed = parseExpression();
                        if (token() != Token::Comma) break;
                        nextToken();
                    }
                    if (token() == Token::RightParen) {
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
                if (token() == Token::Identifier) {
                    auto found( lookUpSymbol( identifier(), scopeNames ));
                    if (found != NullDescriptor()) {
                        storeInstruction( OpPushDescriptor, found );
                    } else {
                        Descriptor value;
                        if (reader->identifierType == VariableType::Local) {
                            value = LocalVariableDescriptor( locals );
                            storeInstruction( OpPushDescriptor, value );
                            nextLocal();
                        } else if (reader->identifierType == VariableType::Argument) {
                            value = ArgumentVariableDescriptor(  reader->integerConstant * sizeof( Descriptor ) );
                            storeInstruction( OpPushDescriptor, value );
                        } else if (reader->identifierType == VariableType::Global) {
                            value = GlobalVariableDescriptor( allocateGlobal( sizeof(Descriptor) ) ) ;
                            storeInstruction( OpPushDescriptor, value );
                        } else fatalError( "Internal parser error - Invalid identifier type" );
                        defineSymbol( identifier(), scopeNames, value );
                    }
                    nextToken();
                } else if (token() == Token::IntegerConstant) {
                    storeInstruction( OpPushInteger, (Word)reader->integerConstant ); nextToken();
                } else if (token() == Token::RealConstant) {
                    storeInstruction( OpPushReal, bit_cast<Word>( reader->realConstant ) ); nextToken();
                } else if (token() == Token::StringConstant) {
                    auto n( reader->stringConstant.size() );
                    auto address( allocateString( n ) );
                    strncpy( (char*)addressString( address ), reader->stringConstant.c_str(), n );
                    storeInstruction( OpPushDescriptor, StringDescriptor( address, n ) );
                    nextToken();
                } else if (token() == Token::LeftParen) {
                    nextToken();
                    auto parsed( parseExpression() );
                    if (token() == Token::RightParen) nextToken();
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
                consumed = false;
                return parsed;
            }

            //  <variable-declaration> ::=
            //      'var' <identifier> [ '(' <expression> ')' ]
            // Declare a variable with an initial value, null no initial value expression is provided.
            bool parseVariable() {
                if (token() == Token::KeywordVar) {
                    nextToken();
                    if (token() == Token::Identifier) {
                        nextToken();
                        // Look-up to see if variable already exists in this scope
                        auto exists( lookUpSymbol( identifier(), scopeNames, true ) );
                        // Create variable in this scope
                        if (exists == NullDescriptor()) {
                            auto descriptor( LocalVariableDescriptor( locals ) );
                            defineSymbol( identifier(), scopeNames, descriptor );
                            nextLocal();
                            storeInstruction( OpPushDescriptor, descriptor );
                            if (token() == Token::LeftParen) {
                                nextToken();
                                parseExpression();
                                if (token() == Token::RightParen) {
                                    nextToken();
                                } else recoverableError( "Expected )", SemiColon );
                            } else {
                                storeInstruction( OpPushNull );
                            }
                            storeInstruction( OpAssign );
                        } else recoverableError( "Identifier already declared", SemiColon );
                    } else recoverableError( "Expected identifier", SemiColon );
                }
                return( false );
            }

            //  <block-statement> ::=
            //      '{' [ <statement> ]* '}'
            bool parseBlock() {
                if (token() == Token::LeftCurly) {
                    nextToken();
                    enterAnonymousScope();
                    auto previousLocals( locals );
                    while ((token() != Token::RightCurly) && (token() != Token::EndOfFile)) parseStatement();
                    locals = previousLocals;
                    exitScope();
                    if (token() == Token::RightCurly) { nextToken(); return true; }
                    else recoverableError( "Expected }", SemiColon );
                }
                return false;
            }
            //  <if-statement> ::=
            //      'if' '(' <expression> ')' <statement> [ 'else' <statement> ]
            bool parseIfStatement() {
                if (token() == Token::KeywordIf) {
                    nextToken();
                    if (token() == Token::LeftParen) {
                        nextToken();
                        parseExpression();
                        auto elseLabel( createPatch() );
                        storeInstruction( OpConditionalJump, Address( 0 ) );
                        consumed = true;
                        if (token() == Token::RightParen) {
                            nextToken();
                            parseStatement();
                            if (token() == Token::KeywordElse) {
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
                if (token() == Token::KeywordWhile) {
                    nextToken();
                    if (token() == Token::LeftParen) {
                        nextToken();
                        auto loopLabel( allocateProgram( 0 ) );
                        parseExpression();
                        auto endLabel( createPatch() );
                        storeInstruction( OpConditionalJump, Address( 0 ) );
                         consumed = true;
                        if (token() == Token::RightParen) {
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
                if (token() == Token::KeywordOut) {
                    nextToken();
                    parseExpression();
                    if (token() == Token::SemiColon) {
                        nextToken();
                        storeInstruction( OpOutput );
                        consumed = true;
                        return true;
                    }
                    else recoverableError( "Expected ;", SemiColon );
                }
                return false;
            }
            //  <return-statement> ::=
            //      'return' [ <expression> ] ';'
            bool parseReturnStatement() {
                if (token() == Token::KeywordReturn) {
                    nextToken();
                    if (token() != Token::SemiColon) {
                        parseExpression();
                    } else {
                        // Procedure returns Null if no expression is provided
                        storeInstruction( OpPushNull );
                    }
                    if (token() == Token::SemiColon) {
                        nextToken();
                        storeInstruction( OpReturn );
                        consumed = true;
                        return true;
                    }
                    else recoverableError( "Expected ;", SemiColon );
                }
                return false;
            }
            //  <procedure-definition> ::=
            //      'def' <local-identifier> [ <named-arguments> ] <statement>
            //  <named-arguments> ::=
            //      '(' <local-identifier> [ ',' <local-identfier> ]* ')'
            // All procedures have a variable number of arguments indexed by $0, $1, .. $N where N is the actual
            // number of argumnts provided. Optionally, arguments can be named. Argument names are aliases for
            // argument indeces.
            // ToDo: provide means of retrieving actual number of arguments via intrinsics (e.g., args() and argv(i) )
            bool parseDefStatement() {
                if (token() == Token::KeywordDef) {
                    nextToken();
                    if (token() == Token::Identifier) {
                        nextToken();
                        // Provisionally Insert code to jump over procedure definition
                        // ToDo: Jump can be avoided by determining start address of code for a translation unit. (low-prio)
                        auto procedureName( identifier() );
                        auto skip( createPatch() );
                        storeInstruction( OpJump, Address( 0 ) );
                        auto value( ProcedureDescriptor( allocateProgram( 0 ) ) );
                        defineSymbol( identifier(), scopeNames, value );
                        enterNamedScope( procedureName );
                        auto previousLocals( locals );
                        auto previousMaxLocals( maxLocals );
                        locals = sizeof( Descriptor );
                        auto opLocals( createPatch() );
                        storeInstruction( OpLocals, Word( locals ) );
                        if (token() == Token::LeftParen) {
                            // Defining named arguments
                            Address index( 0 );
                            nextToken();
                            while (token() == Token::Identifier) {
                                auto argument( ArgumentVariableDescriptor( index++ * sizeof( Descriptor ) ) );
                                defineSymbol( identifier(), scopeNames, argument );
                                nextToken();
                                if (token() != Token::Comma) break;
                                nextToken();
                            }
                            if (token() == Token::RightParen) nextToken();
                            else recoverableError( "Expected )", Token::LeftCurly );
                        }
                        if (token() == Token::LeftCurly) parseBlock();
                        else recoverableError( "Expected {", SemiColon );
                        // ToDo: Only generate return if required (might not be worth the trouble)
                        storeInstruction( OpReturn );
                        patchOperand( opLocals, maxLocals );
                        locals = previousLocals;
                        maxLocals = previousMaxLocals;
                        exitScope();
                        patchJump( skip );
                    } else recoverableError( "Expected identifier", SemiColon );
                }
                return true;
            }
            // <import-statement> ::=
            //      'import' '<' <file-name> '>'
            // <include-statement> ::=
            //      'include' '<' <file-name> '>'
            bool include( bool isImport ) {
                if (token() == Token::LeftAngle) {
                    string fileName;
                    while ((character() != '>') && (character() != '/n') && (character() != EOF)) {
                        fileName += character(); nextCharacter();
                    }
                    if (character() == '>') {
                        nextToken(); // Consume > character
                        nextToken();
                        if (fileName.size() != 0) {
                            bool include( !isImport );
                            if (isImport && !importFileNames.contains( fileName )) {
                                importFileNames.insert( fileName );
                                include = true;
                            }
                            if (include) {
                                // ToDo: Detect and break include cycles
                                pausedReaders.push_back( reader );
                                reader = new Reader( path( fileName ) );
                                nextCharacter();
                                nextToken();
                            }
                            return true;
                        } else recoverableError( "Expected import file name", SemiColon );
                    } else recoverableError( "Expected >", SemiColon );
                } else recoverableError( "Expected <", SemiColon );
                return false;
            }
            bool parseImport() {
                if (token() == Token::KeywordImport) {
                    nextToken();
                    return include( true );
                }
                return false;
            }
            bool parseInclude() {
                if (token() == Token::KeywordInclude) {
                    nextToken();
                    return include( false );
                }
                return false;
            }
            // <statement> ::=
            //      <expression> ';' |
            //      <variable-declaration> |
            //      <block-statement> |
            //      <if-statement> |
            //      <while-statement> |
            //      <output-statement> |
            //      <return-statement> |
            //      <procedure-definition> |
            //      <import-statement> |
            //      <include-statement>
            bool parseStatement() {
                if (token() == Token::KeywordVar) return parseVariable();
                else if (token() == Token::LeftCurly) return parseBlock();
                else if (token() == Token::KeywordIf) return parseIfStatement();
                else if (token() == Token::KeywordWhile) return parseWhileStatement();
                else if (token() == Token::KeywordDef) return parseDefStatement();
                else if (token() == Token::KeywordOut) return parseOutStatement();
                else if (token() == Token::KeywordReturn) return parseReturnStatement();
                else if (token() == Token::KeywordImport) return parseImport();
                else if (token() == Token::KeywordInclude) return parseInclude();
                else {
                    if (!consumed) {
                        storeInstruction( OpPop ); // Consume value of last evaluated expression
                        consumed = true;
                    }
                    if (token() != Token::SemiColon) parseExpression();
                    if (token() == Token::SemiColon) {
                        nextToken();
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
                while (true) {
                    while (token() != Token::EndOfFile) {
                        auto statementOK( parseStatement() );
                        parsed = parsed && statementOK;
                    }
                    if (pausedReaders.size() == 0) break;
                    delete reader;
                    reader = pausedReaders.back();
                    pausedReaders.pop_back();
                }
                return parsed;
            }
            void initialize() {
                locals = 0;
                maxLocals = locals;
                consumed = true;
                errors = 0;
                nextPatch = 0;
                operatorStack.push_back( OpPrecedence( Token::None, -2, 0 ) );
                enterNamedScope( "" );
            }
            
        public:
            TranslateState() = delete;
            TranslateState( const string& source ) : reader( new Reader( source ) ) { initialize(); }
            TranslateState( const path& source ) : reader( new Reader( source ) ) { initialize(); }
            ~TranslateState() { delete reader; }
            Address translate() {
                // ToDo: Start address may not be current program address as code may start with
                // procedure definitions...
                auto start( allocateProgram( 0 ) );
                auto usage( currentMemoryUsage() );
                bool parsed( true );
                try {
                    auto opLocals( createPatch() );
                    storeInstruction( OpLocals, Word( 0 ) );
                    parse();
                    if ((token() == Token::EndOfFile) && (errors == 0)) {
                        // Parsed entire file without errors
                        patchOperand( opLocals, maxLocals );
                        storeInstruction( OpExit );
                        return( start );
                    }
                } catch(...) { return false; }
                if (token() == Token::ReadError) {
                    fatalError( "Read error" );
                }
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
        }; // class TranslateState


    } // namespace unnamed

    Address translate( const string& program ) {
        TranslateState state( program );
        return state.translate();
    }
    Address translate( const path& program ) {
        TranslateState state( program );
        return state.translate();
    }

} // namespace Language

