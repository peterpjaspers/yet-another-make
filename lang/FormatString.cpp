#include "FormatString.h"

#include "Descriptor.h"
#include "Monitor.h"
#include "Instruction.h"
#include "ThreadContext.h"

#include <string>
#include <vector>
#include <iostream>

using namespace std;

namespace Language {

// f_string          ::= (literal_char | "{{" | "}}" | replacement_field)*
// replacement_field ::= "{" f_expression ["="] ["!" conversion] [":" format_spec] "}"
// f_expression      ::= (conditional_expression | "*" or_expr)
//                       ("," conditional_expression | "," "*" or_expr)* [","]
//                       | yield_expression
// conversion        ::= "s" | "r" | "a"
// format_spec       ::= (literal_char | replacement_field)*
// literal_char      ::= <any code point except "{", "}" or NULL>
// format_spec ::= [options][width][grouping]["." precision][type]
// options     ::= [[fill]align][sign]["z"]["#"]["0"]
// fill        ::= <any character>
// align       ::= "<" | ">" | "=" | "^"
// sign        ::= "+" | "-" | " "
// width       ::= digit+
// grouping    ::= "," | "_"
// precision   ::= digit+
// type        ::= "b" | "c" | "d" | "e" | "E" | "f" | "F" | "g" | "G" | "n" | "o" | "s" | "x" | "X" | "%"

    enum FieldAlignment { Left, Right, Pad, Center };

    // Chop format-string in sequence of sub-strings and replacement-field strings.
    // The replacement-field string may itself contain (single) nested replacement-fields.
    void chop( const string source, vector<string>& subStrings, vector<string>& fields ) {
        static const char* signature( "void chop( const string source, vector<string> subStrings )" );
        enum State { Top, Field, Nested } state( Top );
        auto n( source.size() );
        int p( 0 );
        string subString;
        string field;
        while (p < n) {;
            auto c( source[ p++ ]);
            switch (state) {
            case State::Top:
                if (c != '{') subString.push_back( c );
                else if ((p < n) && (source[ p ] == '{')) continue;
                else { subStrings.push_back( subString ); subString.clear(); state = State::Field; }
                break;
            case State::Field:
                if (c != '}') field.push_back( c );
                else if (c == '{') state = State::Nested;
                else { fields.push_back( field ); field.clear(); state = State::Top; }
                break;
            case Nested:
                if (c == '{') throw string( signature ) + " - Invalid field syntax, excessive nesting";
                field.push_back( c );
                if (c == '}') state = Field;
                break;
            }
        }
        if (state == State::Top) subStrings.push_back( subString );
        else fields.push_back( field );
    }
    string evaluateFieldExpression( ThreadContext& ctx, string field ) {
        evaluateExpression( ctx, field );
        auto descriptor( pop() );
        descriptor = dereference( ctx, descriptor );
        return stringToCString( ctx, toString( ctx, descriptor ) );
    }
    string evaluateReplacementField( ThreadContext& ctx, string field ) {
        // ToDo: reclaim string memory
        vector<string> subStrings;
        vector<string> fields;
        chop( field, subStrings, fields );
        if (0 < fields.size()) {
            for ( auto field : fields ) { field = evaluateFieldExpression( ctx, field ); }
            field.clear();
            for ( int i = 0; i < subStrings.size(); ++i ) {
                field.append( subStrings[ i ] );
                if (i < fields.size()) field.append( fields[ i ] );
            }
        } else {
            field = evaluateFieldExpression( ctx, field );
        }
        return field;
    }
    bool isFormatString( const ThreadContext& ctx, const Descriptor string ) {
        if (isString( string )) {
            const char* content( reinterpret_cast<char*>( addressString( ctx, string ) ) );
            Word length( type( string ) & TypeValueMask );
            for ( Word i = 0; i < length; ++i ) if ( content[ i ] == '{' ) return true;
        }
        return false;
    }
    Descriptor evaluateFormatString( ThreadContext& ctx, const Descriptor source ) {
        auto formatString( stringToCString( ctx, source ) );
        string formattedString( "" );
        vector<string> subStrings;
        vector<string> fields;
        chop( formatString, subStrings, fields );
        for ( auto& field : fields ) field = evaluateReplacementField( ctx, field );
        for (int i = 0; i < subStrings.size(); ++i) {
            formattedString.append( subStrings[ i ] );
            if (i < fields.size()) formattedString.append( fields[ i ] );
        }
        return toStringDescriptor( ctx, formattedString );
    }

} // namespace Language
