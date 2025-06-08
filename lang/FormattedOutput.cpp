#include "FormattedOutput.h"

#include "Descriptor.h"
#include "Monitor.h"

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
    Descriptor formattedOutput( int argc, Descriptor* argv ) {
        if (monitor( DebugAspects::IntrinsicCalls )) monitorRecord() << "Intrinsic : output( " << argc << " ... )" << record<char>;
        for ( int i = 0; i < argc; ++i ) {
            auto value( argv[ i ] );
            auto str( stringToCString( toString( value ) ) );
            cout << str;
        }
        cout << endl;
        return NullDescriptor();
    }

} // namespace Language
