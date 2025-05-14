#include "Reader.h"
#include "Monitor.h"

#include <map>
#include <fstream>
#include <sstream>

using namespace std;
using namespace std::filesystem;

namespace Language {

    namespace {

        const map<string,Token> keywords = {
            { string( "var" ), KeywordVar },
            { string( "if" ), KeywordIf },
            { string( "else" ), KeywordElse },
            { string( "while" ), KeywordWhile },
            { string( "def" ), KeywordDef },
            { string( "return" ), KeywordReturn },
            { string( "out" ), KeywordOut },
            { string( "include" ), KeywordInclude },
            { string( "import" ), KeywordImport },
        };

    }

    Reader::Reader( const std::string& source ) :
        stream( new istringstream( source ) ),
        fileName( "Anonymous" ),
        line( "" ),
        lineNumber( 0 ),
        characterPosition( 0 ),
        character( ' ' ),
        token( Token::None ),
        identifierType( VariableType::Undefined )
    {}
    Reader::Reader( const std::filesystem::path& source ) :
        stream( new ifstream( source ) ),
        fileName( source.string() ),
        line( "" ),
        lineNumber( 0 ),
        characterPosition( 0 ),
        character( ' ' ),
        token( Token::None ),
        identifierType( VariableType::Undefined )
    {};
    Reader::~Reader() { delete stream; }

    // Read (next) line from program source
    void Reader::nextLine() {
        if (!stream->good()) throw "Error reading source file";
        getline( *stream, line );
        lineNumber += 1;
        if (monitor( DebugAspects::SourceCode )) monitorRecord() << setw( 6 ) << lineNumber << " : " << line << record<char>;
        characterPosition = 0;            
    }
    // Read next character from source, reading a new line from the source as required 
    void Reader::nextCharacter() {
        while (line.size() <= characterPosition) {
            if (stream->eof()) { character = EOF; break; }
            else if (stream->good()) nextLine();
            else { character = EOF; token = Token::ReadError; };
        }
        if (character != EOF) character = line[ characterPosition++ ];
    }
    void Reader::skipWhiteSpace() { while (isspace( static_cast<unsigned char>( character ) ) && (character != EOF)) nextCharacter(); }
    inline bool isIdentifier( char c ) { return( isalpha( c ) || (c == '_') ); }
    inline bool isComment( const string& line, int pos ) {
        if (((pos + 1) < line.size()) && (line[ pos ] == '/') && (line[ pos + 1 ] == '/')) return true;
        return false;
    }
    // ToDo: Table driven tokenizer to avoid large if then else construct
    void Reader::nextToken() {
        skipWhiteSpace();
        if (isIdentifier( character )) {
            identifier.clear();
            identifier += character;
            nextCharacter();
            while (isIdentifier( character )) { identifier += character; nextCharacter(); }
            auto keyword( keywords.find( identifier ) );
            if (keyword == keywords.end()) {
                token = Token::Identifier;
                identifierType = VariableType::Local;
            } else token = keyword->second;
        } else if (character == ':') {
            nextCharacter();
            if (isIdentifier( character )) {
                while (isIdentifier( character )) { identifier += character; nextCharacter(); }
                token = Token::Identifier;
                identifierType = VariableType::Global;
            } else {
                token = Token::Colon;
            }
        } else if (character == '$') {
            nextCharacter();
            if (isdigit( character )) {
                string number;
                while (isdigit( character )) { identifier += character; number += character; nextCharacter(); }
                integerConstant = stoi( number );
                token = Token::Identifier;
                identifierType = VariableType::Argument;
            } else {
                token = Token::Dollar;
            }
        } else if (isdigit( character )) {
            // Parse integer or real
            string number;
            while (isdigit( character )) { number += character; nextCharacter(); }
            if (character == '.') {
                // Parsing real
                number += character; nextCharacter();
                while (isdigit( character )) { number += character; nextCharacter(); }
                realConstant = stof( number );
                token = Token::RealConstant;
            } else {
                integerConstant = stoi( number );
                token = Token::IntegerConstant;
            }
        } else if (character == '\"' ) {
            // Parse string constant
            stringConstant.clear();
            nextCharacter();
            while (character != '\"') {
                if (character == '\\') { nextCharacter(); }
                stringConstant += character;
                nextCharacter();
            }
            nextCharacter();
            token = Token::StringConstant;
        } else if (character == '?' ) { token = Token::Question; nextCharacter(); }
        else if (character == ':' ) { token = Token::Colon; nextCharacter(); }
        else if (character == ',' ) { token = Token::Comma; nextCharacter(); }
        else if (character == ';' ) { token = Token::SemiColon; nextCharacter(); }
        else if (character == '.' ) { token = Token::Period; nextCharacter(); }
        else if (character == '(' ) { token = Token::LeftParen; nextCharacter(); }
        else if (character == ')' ) { token = Token::RightParen; nextCharacter(); }
        else if (character == '[' ) { token = Token::LeftSquare; nextCharacter(); }
        else if (character == ']' ) { token = Token::RightSquare; nextCharacter(); }
        else if (character == '{' ) { token = Token::LeftCurly; nextCharacter(); }
        else if (character == '}' ) { token = Token::RightCurly; nextCharacter(); }
        else if (character == '\'' ) { token = Token::SingleQuote; nextCharacter(); }
        else if (character == '"' ) { token = Token::DoubleQuote; nextCharacter(); }
        else if (character == '`' ) { token = Token::Apostrophe; nextCharacter(); }
        else if (character == '<' ) {
            nextCharacter();
            if (character == '<' ) {
                if (character == '=' ) { token = Token::LeftLeftAngleEqual; nextCharacter(); }
                else { token = Token::LeftLeftAngle; nextCharacter(); }
            } else if (character == '=' ) { token = Token::LeftAngleEqual; nextCharacter(); }
            else token = Token::LeftAngle;
        } else if (character == '>' ) {
            nextCharacter();
            if (character == '>' ) {
                if (character == '=' ) { token = Token::RightRightAngleEqual; nextCharacter(); }
                else { token = Token::RightRightAngle; nextCharacter(); }
            } else if (character == '=' ) { token = Token::RightAngleEqual; nextCharacter(); }
            else token = Token::RightAngle;
        } else if (character == '!') {
            nextCharacter();
            if (character == '=' ) { token = Token::ExclamationEqual; nextCharacter(); }
            else token = Token::Exclamation;
        } else if (character == '+') {
            nextCharacter();
            if (character == '=' ) { token = Token::PlusEqual; nextCharacter(); }
            else token = Token::Plus;
        } else if (character == '-') {
            nextCharacter();
            if (character == '=' ) { token = Token::MinusEqual; nextCharacter(); }
            else token = Token::Minus;
        } else if (character == '*') {
            nextCharacter();
            if (character == '=' ) { token = Token::AsteriskEqualSign; nextCharacter(); }
            else token = Token::Asterisk;
        } else if (character == '!') {
            nextCharacter();
            if (character == '=' ) { token = Token::ExclamationEqual; nextCharacter(); }
            else token = Token::Exclamation;
        } else if (character == '&') {
            nextCharacter();
            if (character == '&' ) { token = Token::AmpersandAmpersand; nextCharacter(); }
            if (character == '=' ) { token = Token::AnpersandEqual; nextCharacter(); }
            else token = Token::Ampersand;
        } else if (character == '/') {
            nextCharacter();
            if (character == '=' ) { token = Token::SlashEqual; nextCharacter(); }
            else if (character == '/') {
                characterPosition -= 1;
                while (isComment( line, (characterPosition - 1))) {
                    nextLine();
                    nextCharacter();
                    skipWhiteSpace();
                }
                nextToken();
            }
            else token = Token::Slash;
        } else if (character == '|') {
            nextCharacter();
            if (character == '|' ) { token = Token::VerticalVerticalBar; nextCharacter(); }
            else if (character == '=' ) { token = Token::VerticalBarEqual; nextCharacter(); }
            else token = Token::VerticalBar;
        } else if (character == '=') {
            nextCharacter();
            if (character == '=' ) { token = Token::EqualEqual; nextCharacter(); }
            else token = Token::Equal;
        } else if (character == '^') {
            nextCharacter();
            if (character == '=' ) { token = Token::CaretEqual; nextCharacter(); }
            else token = Token::Caret;
        } else if (character == '%') {
            nextCharacter();
            if (character == '=' ) { token = Token::PercentileEqual; nextCharacter(); }
            else token = Token::Percentile;
        } else if (character == '~') {
            nextCharacter();
            token = Token::Tilde;
        } else if (character == '@') {
            nextCharacter();
            token = Token::AtSign;
        } else if (character == '\\') {
            nextCharacter();
            token = Token::BackSlash;
        } else if (character == EOF) {
            if (token != Token::ReadError) token = Token::EndOfFile;
        } else {
            token = Token::Illegal;
        }
    };
    void Reader::skipToToken( const Token to ) { while ((token != to) && (token != EndOfFile)) nextToken(); }
    
}