#include "Intrinsics.h"
#include "SymbolTable.h"
#include "Instruction.h"
#include "Monitor.h"
#include "Translator.h"

#include <iostream>

using namespace std;

namespace Language {

    typedef uint32_t Intrinsic;
    static const Intrinsic IntrinOutput             = Intrinsic(0U);

    IntrinsicFunction output;

    namespace {

        const SymbolTable intrinsics = {
            { string( "out" ),      IntrinsicDescriptor( IntrinOutput ) },
        };

        IntrinsicFunction* functions[] = {
            &output,
        };
    
    }

    IntrinsicFunction* intrinsic( const Descriptor& descriptor ) {
        return functions[ Word( descriptor ) ];
    }
    
    void defineIntrinsics() { for( auto i : intrinsics ) defineSymbol( i.first, i.second ); }

    // Output expression value to console
    Descriptor output( int argc, Descriptor* argv ) {
        if (monitor( DebugAspects::IntrinsicCalls )) monitorRecord() << "Intrinsic : output( " << argc << " ... )" << record<char>;
        for ( int i = 0; i < argc; ++i ) {
            auto value( argv[ i ] );
            auto str( stringToCString( toString( value ) ) );
            cout << str;
        }
        cout << endl;
        return NullDescriptor();
    }
}
