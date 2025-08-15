#include "Translator.h"

#include "Reader.h"
#include "SymbolTable.h"
#include "Instruction.h"
#include "Monitor.h"
#include "FormattedOutput.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <set>

// ToDo: Python like format strings
// ToDo: Better error handling; i.e., try to recover
// ToDo: Optional parentheses for single argument procedure call: i.e., "out( arg )" with parens or "out arg" without

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
            // Current extent of local variable descriptors relative to frame-pointer (fp)
            Word localBase;
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
            // Block indexes numbers successive nested stament blocks.
            // This is required to uniquely identify the current (annonymous) scope.
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
            PatchLabel createPatch( ThreadContext& ctx ) {
                if (nextPatch == MAXINT32) fatalError( "Out of jump labels" );
                auto label( nextPatch++ );
                patches.insert( { label, allocateProgram( ctx, 0 ) } );
                return label;
            }
            // Patch (set) the operand value of the (previously generated) instruction associated with a patch label
            void patchOperand( ThreadContext& ctx, const PatchLabel label, const Word operand ) {
                auto entry( patches.find( label ) );
                if (entry == patches.end()) fatalError( "Internal error, invalid patch label" );
                auto opAddress( entry->second );
                auto operandAddress( reinterpret_cast<Word*>( addressProgram( ctx, opAddress ) + sizeof( OpCode ) ) );
                *operandAddress = operand;
            }
            // Set target address of the jump instruction associated with a patch label to the current program address.
            void patchJump( ThreadContext& ctx, const PatchLabel label ) { patchOperand( ctx, label, allocateProgram( 0 ) ); }

            void parseEnterNamedScope( ThreadContext& ctx, const string name ) {
                blockIndeces.push_back( 0 );
                auto n( name.size() );
                auto address( allocateString( ctx, n ) );
                strncpy( (char*)addressString( ctx, address ), name.c_str(), n );
                enterNamedScope( ctx, name );
                storeInstruction( ctx, OpEnterNamedScope, StringDescriptor( address, n ) );
            }
            void parseEnterAnonymousScope( ThreadContext& ctx ) {
                static const char* signature( "void parseEnterAnonymousScope()" );
                if (blockIndeces.size() == 0) throw string( signature ) + "Internal parser error, no scope defined";
                auto index( blockIndeces.back() );
                blockIndeces.pop_back();
                blockIndeces.push_back( index + 1 );
                blockIndeces.push_back( 0 );
                enterAnonymousScope( ctx, index );
                storeInstruction( ctx, OpEnterScope, Word( index ) );
            }
            void parseExitScope( ThreadContext& ctx ) {
                static const char* signature( "void parseExitScope()" );
                if (blockIndeces.size() == 0) throw string( signature ) + "Internal parser error, no scope defined";
                blockIndeces.pop_back();
                exitScope( ctx );
                storeInstruction( ctx, OpExitScope );
            }

            // Conditionally drop last (unconsumed) expression value by popping it from the stack
            void dropExpressionValue( ThreadContext& ctx ) {
                if (!consumed) {
                    storeInstruction( ctx, OpPop ); // Consume value of last evaluated expression
                    consumed = true;
                }
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
            void generateExpressionCode(  ThreadContext& ctx, const OpPrecedence op ) {
                auto top( operatorStack.back() );
                while (top.precedence >= op.precedence) { storeInstruction( ctx, top.code ); operatorStack.pop_back(); top = operatorStack.back(); }
                if (op.token != Token::None) operatorStack.push_back( op );
            }
            inline void generateExpressionCode( ThreadContext& ctx ) { generateExpressionCode( ctx, OpPrecedence{ Token::None, -1, 0 } ); }
            inline void suspendExpressionCode() { operatorStack.push_back( OpPrecedence{ Token::None, -2, 0 } ); }
            inline void resumeExpressionCode( ThreadContext& ctx ) { generateExpressionCode( ctx ); operatorStack.pop_back(); }
            bool parseDyadicExpression( ThreadContext& ctx ) {
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
                auto parsed( parseMonadicExpression( ctx ) );
                if (parsed) {
                    bool parseAssignment( true );
                    for (int i = 0; i < (sizeof( ops ) / sizeof( OpPrecedence )); ++i) {
                        if ((ops[ i ].token) == token()) {
                            nextToken();
                            if (ops[ i ].code != 0) generateExpressionCode( ctx, ops[ i ] );
                            parsed = parseDyadicExpression( ctx );
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
                            parsed = parseDyadicExpression( ctx );
                            if (parsed) generateExpressionCode( ctx, { Equal, 0, OpAssign } );
                        } else {
                            for (int i = 0; i < (sizeof( asignOps ) / sizeof( OpPrecedence )); ++i) {
                                if ((asignOps[ i ].token) == token()) {
                                    nextToken();
                                    storeInstruction( ctx, OpDup );
                                    parsed = parseDyadicExpression( ctx );
                                    if (asignOps[ i ].code != 0) generateExpressionCode( ctx, asignOps[ i ] );
                                    if (parsed) {
                                        generateExpressionCode( ctx, { Equal, 0, OpAssign } );
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
            bool parseMonadicExpression( ThreadContext& ctx ) {
                static const OpPrecedence ops[] = {
                    { Plus, 11, 0 }, { Minus, 11, OpNegate },   // arithmetic
                    { Tilde, 11, OpInvert },                    // bit-wise
                    { Exclamation, 11, OpNot }                  // logical
                };
                if (monitor( DebugAspects::ParserFunctions )) monitorRecord() << setw( 20 ) << "" << "parseMonadicExpression" << record<char>;
                for (int i = 0; i < (sizeof( ops ) / sizeof( Token )); ++i) {
                    if (ops[ i ].token == token()) {
                        nextToken();
                        auto parsed( parseMonadicExpression( ctx ) );
                        if (ops[ i ].code != 0) generateExpressionCode( ctx, ops[ i ] );
                        return parsed;
                    }
                }
                return parsePostfixExpression( ctx );
            }
            // <postfix> ::= <primary> [ <index> | <arguments> ]
            // <index> ::= '[' <expression> ']'
            // <arguments> ::= '(' <expression> [ ',' <expression> ]+ ')'
            bool parsePostfixExpression( ThreadContext& ctx ) {
                if (monitor( DebugAspects::ParserFunctions )) monitorRecord() << setw( 20 ) << "" << "parsePostfixExpression" << record<char>;
                auto parsed( parsePrimaryExpression( ctx ) );
                if (parsed && (token() == Token::RightSquare)) {
                    nextToken();
                    parsed = parseExpression( ctx );
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
                    storeInstruction( ctx, OpArguments );
                    // Parse argument expressions
                    while (parsed) {
                        parsed = parseExpression( ctx );
                        // Dereference argument expressions that result in variable references prior to entering
                        // procedure as (local) variable scope will be inaccessible inside the procedure.
                        // This is only strictly required for local variables (not values or global variables)
                        // but this is not known at compile time. The dereference is a no-op for values.
                        storeInstruction( ctx, OpDereference );
                        if (token() != Token::Comma) break;
                        nextToken();
                    }
                    if (token() == Token::RightParen) {
                        nextToken();
                        storeInstruction( ctx, OpProcedureCall );
                        // ... resume (lower-priority) expression evaluation
                        resumeExpressionCode( ctx );
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
            bool parsePrimaryExpression( ThreadContext& ctx ) {
                if (monitor( DebugAspects::ParserFunctions )) monitorRecord() << setw( 20 ) << "" << "parsePrimaryExpression" << record<char>;
                if (token() == Token::Identifier) {
                    auto found( lookUpSymbol( ctx, identifier() ));
                    if (found != NullDescriptor()) {
                        storeInstruction( OpPushDescriptor, found );
                    } else {
                        recoverableError( "Undefined identifier ", SemiColon );
                    }
                    nextToken();
                } else if (token() == Token::IntegerConstant) {
                    storeInstruction( ctx, OpPushInteger, (Word)reader->integerConstant ); nextToken();
                } else if (token() == Token::RealConstant) {
                    storeInstruction( ctx, OpPushReal, bit_cast<Word>( reader->realConstant ) ); nextToken();
                } else if (token() == Token::StringConstant) {
                    storeInstruction( ctx, OpPushDescriptor, toStringDescriptor( ctx, reader->stringConstant ) );
                    nextToken();
                } else if (token() == Token::LeftParen) {
                    nextToken();
                    auto parsed( parseExpression( ctx ) );
                    if (token() == Token::RightParen) nextToken();
                    else { recoverableError( "Expected )", SemiColon ); parsed = false; }
                    return parsed;
                } else {
                    recoverableError( "Expected expression", SemiColon );
                    return false;
                }
                return true;
            }
            bool parseExpression( ThreadContext& ctx ) {
                if (monitor( DebugAspects::ParserFunctions )) monitorRecord() << setw( 20 ) << "" << "parseExpression" << record<char>;
                dropExpressionValue( ctx );
                auto parsed( parseDyadicExpression( ctx ) );
                generateExpressionCode( ctx );
                consumed = false;
                return parsed;
            }

            //  <variable-declaration> ::=
            //      'var' <identifier> [ '(' <expression> ')' ]
            // Declare a variable with an initial value, null no initial value expression is provided.
            bool parseVariable( ThreadContext& ctx ) {
                if (token() == Token::KeywordVar) {
                    dropExpressionValue( ctx );
                    nextToken();
                    if (token() == Token::Identifier) {
                        nextToken();
                        // Look-up to see if variable already exists in the current scope.
                        auto exists( lookUpSymbol( ctx, identifier(), true ) );
                        if (exists == NullDescriptor()) {
                            // Create variable in current scope
                            auto descriptor( LocalVariableDescriptor( locals + localBase ) );
                            defineSymbol( ctx, identifier(), descriptor );
                            nextLocal();
                            storeInstruction( ctx, OpPushDescriptor, descriptor );
                            if (token() == Token::LeftParen) {
                                nextToken();
                                parseExpression( ctx );
                                if (token() == Token::RightParen) {
                                    nextToken();
                                } else recoverableError( "Expected )", SemiColon );
                            } else {
                                storeInstruction( ctx, OpPushNull );
                            }
                            storeInstruction( ctx, OpAssign );
                        } else recoverableError( "Identifier already declared", SemiColon );
                    } else recoverableError( "Expected identifier", SemiColon );
                }
                return( false );
            }

            //  <block-statement> ::=
            //      '{' [ <statement> ]* '}'
            bool parseBlock( ThreadContext& ctx, bool scoped ) {
                if (token() == Token::LeftCurly) {
                    dropExpressionValue( ctx );
                    nextToken();
                    if (scoped) parseEnterAnonymousScope( ctx );
                    auto previousLocals( locals );
                    while ((token() != Token::RightCurly) && (token() != Token::EndOfFile)) {
                        parseStatement( ctx );
                        if (token() == Token::SemiColon) nextToken();
                    }
                    locals = previousLocals;
                    if (scoped) parseExitScope( ctx );
                    if (token() == Token::RightCurly) { nextToken(); return true; }
                    else recoverableError( "Expected }", SemiColon );
                }
                return false;
            }
            //  <if-statement> ::=
            //      'if' '(' <expression> ')' <statement> [ 'else' <statement> ]
            bool parseIfStatement( ThreadContext& ctx ) {
                if (token() == Token::KeywordIf) {
                    dropExpressionValue( ctx );
                    nextToken();
                    if (token() == Token::LeftParen) {
                        nextToken();
                        parseExpression( ctx );
                        auto elseLabel( createPatch( ctx ) );
                        storeInstruction( ctx, OpConditionalJump, Address( 0 ) );
                        consumed = true;
                        if (token() == Token::RightParen) {
                            nextToken();
                            parseStatement( ctx );
                            if (token() == Token::SemiColon) nextToken();
                            if (token() == Token::KeywordElse) {
                                nextToken();
                                auto endLabel( createPatch( ctx ) );
                                storeInstruction( OpJump, Address( 0 ) );
                                patchJump( ctx, elseLabel );
                                parseStatement( ctx );
                                if (token() == Token::SemiColon) nextToken();
                                patchJump( ctx, endLabel );
                            } else {
                                patchJump( ctx, elseLabel );
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
            bool parseWhileStatement( ThreadContext& ctx ) {
                if (token() == Token::KeywordWhile) {
                    dropExpressionValue( ctx );
                    nextToken();
                    if (token() == Token::LeftParen) {
                        nextToken();
                        auto loopLabel( allocateProgram( ctx, 0 ) );
                        parseExpression( ctx );
                        auto endLabel( createPatch( ctx ) );
                        storeInstruction( OpConditionalJump, Address( 0 ) );
                        consumed = true;
                        if (token() == Token::RightParen) {
                            nextToken();
                            parseStatement( ctx );
                            if (token() == Token::SemiColon) nextToken();
                            storeInstruction( ctx, OpJump, loopLabel );
                            patchJump( ctx, endLabel );
                        } else {
                            recoverableError( "Expected )", SemiColon );
                        }
                    } else {
                        recoverableError( "Expected (", SemiColon );
                    }
                }
                return true;
            }
            //  <return-statement> ::=
            //      'return' [ <expression> ] ';'
            bool parseReturnStatement( ThreadContext& ctx ) {
                if (token() == Token::KeywordReturn) {
                    nextToken();
                    if (token() != Token::SemiColon) {
                        parseExpression( ctx );
                    } else {
                        // Procedure returns Null if no expression is provided
                        dropExpressionValue( ctx );
                        storeInstruction( OpPushNull );
                    }
                    if (token() == Token::SemiColon) {
                        nextToken();
                        storeInstruction( ctx, OpExitScope );
                        storeInstruction( ctx, OpReturn );
                        consumed = true;
                        return true;
                    }
                    else recoverableError( "Expected ;", SemiColon );
                }
                return false;
            }
            //  <evaluate-statement> ::=
            //      'eval' [ <expression> ] ';'
            bool parseEvaluateStatement( ThreadContext& ctx ) {
                if (token() == Token::KeywordEval) {
                    dropExpressionValue( ctx );
                    nextToken();
                    parseExpression( ctx );
                    if (token() == Token::SemiColon) {
                        nextToken();
                        storeInstruction( ctx, OpEvaluate );
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
            // number of argumnts provided.
            // Optionally, arguments can be named. Argument names are aliases for argument indexes.
            bool parseDefStatement( ThreadContext& ctx ) {
                if (token() == Token::KeywordDef) {
                    dropExpressionValue( ctx );
                    nextToken();
                    if (token() == Token::Identifier) {
                        nextToken();
                        // Provisionally Insert code to jump over procedure definition
                        // ToDo: Jump can be avoided by determining start address of code for a translation unit. (low-prio)
                        auto procedureName( identifier() );
                        // Look-up to see if procedure is defined in the current scope.
                        auto exists( lookUpSymbol( ctx, procedureName, true ) );
                        if (exists == NullDescriptor()) {
                            // Define procedure in current scope
                            auto skip( createPatch( ctx ) );
                            storeInstruction( ctx, OpJump, Address( 0 ) );
                            auto value( ProcedureDescriptor( allocateProgram( 0 ) ) );
                            defineSymbol( ctx, identifier(), value );
                            parseEnterNamedScope( ctx, procedureName );
                            auto previousLocals( locals );
                            auto previousMaxLocals( maxLocals );
                            locals = sizeof( Descriptor );
                            auto localsPatch( createPatch( ctx ) );
                            storeInstruction( ctx, OpLocals, Word( locals ) );
                            if (token() == Token::LeftParen) {
                                // Defining named arguments
                                Address index( 0 );
                                nextToken();
                                while (token() == Token::Identifier) {
                                    auto argument( ArgumentVariableDescriptor( index++ * sizeof( Descriptor ) ) );
                                    defineSymbol( ctx, identifier(), argument );
                                    nextToken();
                                    if (token() != Token::Comma) break;
                                    nextToken();
                                }
                                if (token() == Token::RightParen) nextToken();
                                else recoverableError( "Expected )", Token::LeftCurly );
                            }
                            if (token() == Token::LeftCurly) parseBlock( ctx, false );
                            else recoverableError( "Expected {", SemiColon );
                            patchOperand( ctx, localsPatch, maxLocals );
                            locals = previousLocals;
                            maxLocals = previousMaxLocals;
                            parseExitScope( ctx );
                            // ToDo: Only generate return if required (might not be worth the trouble)
                            storeInstruction( ctx, OpReturn );
                            patchJump( ctx, skip );
                        } else {
                            recoverableError( "Procedure already defined", SemiColon );
                        }
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
            bool parseImport( ThreadContext& ctx ) {
                if (token() == Token::KeywordImport) {
                    dropExpressionValue( ctx );
                    nextToken();
                    return include( true );
                }
                return false;
            }
            bool parseInclude( ThreadContext& ctx ) {
                if (token() == Token::KeywordInclude) {
                    dropExpressionValue( ctx );
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
            //      <return-statement> |
            //      <evaluate-statement> |
            //      <procedure-definition> |
            //      <import-statement> |
            //      <include-statement>
            bool parseStatement( ThreadContext& ctx ) {
                if (token() == Token::KeywordVar) return parseVariable( ctx );
                else if (token() == Token::LeftCurly) return parseBlock( ctx, true );
                else if (token() == Token::KeywordIf) return parseIfStatement( ctx );
                else if (token() == Token::KeywordWhile) return parseWhileStatement( ctx );
                else if (token() == Token::KeywordDef) return parseDefStatement( ctx );
                else if (token() == Token::KeywordReturn) return parseReturnStatement( ctx );
                else if (token() == Token::KeywordEval) return parseEvaluateStatement( ctx );
                else if (token() == Token::KeywordImport) return parseImport( ctx );
                else if (token() == Token::KeywordInclude) return parseInclude( ctx );
                else {
                    if (token() == Token::EndOfFile) return true;
                    if (token() == Token::SemiColon) {
                        nextToken();
                        return true;
                    }
                    return parseExpression( ctx );
                }
                return false;
            }
            // <program> ::= [ <statement> ]*
            bool parse( ThreadContext& ctx ) {
                if (monitor( DebugAspects::ParserFunctions )) monitorRecord() << setw( 20 ) << "" << "parse" << record<char>;
                bool parsed( true );
                nextCharacter();
                nextToken();
                while (true) {
                    while (token() != Token::EndOfFile) {
                        auto statementOK( parseStatement( ctx ) );
                        if (token() == Token::SemiColon) nextToken();
                        parsed = parsed && statementOK;
                    }
                    if (pausedReaders.size() == 0) break;
                    delete reader;
                    reader = pausedReaders.back();
                    pausedReaders.pop_back();
                }
                return parsed;
            }
            void initialize( Word base ) {
                localBase = base;
                locals = 0;
                maxLocals = 0;
                consumed = true;
                errors = 0;
                nextPatch = 0;
                operatorStack.push_back( OpPrecedence( Token::None, -2, 0 ) );
            }
            
        public:
            TranslateState() = delete;
            TranslateState( const string& source, Word base ) : reader( new Reader( source ) ) { initialize( base ); }
            TranslateState( const path& source, Word base ) : reader( new Reader( source ) ) { initialize( base ); }
            ~TranslateState() { delete reader; }
            Address translate( ThreadContext& ctx ) {
                // ToDo: Start address may not be current program address as code may start with
                // procedure definitions...
                auto start( allocateProgram( ctx, 0 ) );
                auto usage( currentMemoryUsage( ctx ) );
                bool parsed( true );
                try {
                    parseEnterNamedScope( ctx, "" );
                    defineIntrinsic( "out", formattedOutput );
                    auto localsPatch( createPatch( ctx ) );
                    storeInstruction( ctx, OpLocals, Word( 0 ) );
                    parse( ctx );
                    if ((token() == Token::EndOfFile) && (errors == 0)) {
                        // Parsed entire file without errors
                        patchOperand( ctx, localsPatch, maxLocals );
                        parseExitScope( ctx );
                        storeInstruction( ctx, OpExit );
                        return( start );
                    }
                } catch(...) { return false; }
                if (token() == Token::ReadError) {
                    fatalError( "Read error" );
                }
                if (errors != 0) {
                    // Parsing failed, reclaim provisionally allocated memory
                    recoverMemory( ctx, usage );
                    // Replace failed translation unit with (procedure) code returning null
                    storeInstruction( ctx, OpLocals, Word(0) );
                    storeInstruction( ctx, OpPushNull );
                    storeInstruction( ctx, OpReturn );
                };
                return start;
            }
        }; // class TranslateState


    } // namespace unnamed

    Address translate( ThreadContext& ctx, const string& program, Word offset ) {
        TranslateState state( program, offset );
        return state.translate( ctx  );
    }
    Address translate( ThreadContext& ctx, const path& program, Word offset ) {
        TranslateState state( program, offset );
        return state.translate( ctx );
    }

} // namespace Language

