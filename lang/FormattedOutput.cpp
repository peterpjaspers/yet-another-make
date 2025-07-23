#include "FormattedOutput.h"

#include "Descriptor.h"
#include "Monitor.h"

#include <string>
#include <iostream>

using namespace std;

namespace Language {

    Descriptor formattedOutput( ThreadContext& ctx, int argc, Descriptor* argv ) {
        if (monitor( DebugAspects::IntrinsicCalls )) {
            auto& recording( monitorRecord() );
            recording << "Intrinsic : output( " << argc;
            for (int i = 0; i < argc; ++ i) recording << ", " << toReadable( argv[ i ] );
            recording << " )" << record<char>;
        }
        for ( int i = 0; i < argc; ++i )  cout << stringToCString( ctx, toString( ctx, argv[ i ] ) );
        cout << endl;
        return NullDescriptor();
    }

} // namespace Language
