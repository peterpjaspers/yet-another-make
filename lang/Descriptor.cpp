#include "Descriptor.h"
#include "ThreadContext.h"

using namespace std;

namespace Language {

    Descriptor dereference( ThreadContext& ctx, const Descriptor& descriptor ) {
        static const char* signature( "Descriptor dereference( const Descriptor& descriptor )" );
        auto typ( type( descriptor ) );
        auto adr( address( descriptor ) );
        if (isLocalVariable( descriptor)) return *addressLocal( ctx, adr );
        if (isArgumentVariable( descriptor)) return *addressArgument( ctx, adr );
        if (isGlobalVariable( descriptor)) return *addressGlobal( ctx, adr );
        return descriptor;
    }
    Descriptor& toInteger( Descriptor& d ) {
        static const char* signature( "void toInteger( Descriptor& )" );
        auto type( typeCode( d ) );
        if (type != TypeInteger) {
            if (type == TypeNull) d = IntegerDescriptor( 0 );
            else if (type == TypeReal) d = IntegerDescriptor( lround( real( d ) ) );
            else if (type == TypeString) {
                toNumeric( d );
                if (typeCode( d ) == TypeReal) d = IntegerDescriptor( lround( real( d ) ) );
                else if (typeCode( d ) != TypeInteger) throw string( signature ) + " - Cannot convert " + toReadable( d ) + " to integer.";
            }
            else throw string( signature ) + " - Cannot convert " + toReadable( d ) + " to integer.";
        }
        return( d );
    }
    Descriptor& toReal( Descriptor& d ) {
        static const char* signature( "void toReal( Descriptor& )" );
        auto type( typeCode( d ) );
        if (type != TypeReal) {
            if (type == TypeNull) d = RealDescriptor( 0.0 );
            else if (type == TypeInteger) d = RealDescriptor( (float)integer( d ) );
            else if (type == TypeString) {
                toNumeric( d );
                if (typeCode( d ) == TypeInteger) d = RealDescriptor( integer( d ) );
                else if (typeCode( d ) != TypeReal) throw string( signature ) + " - Cannot convert " + toReadable( d ) + " to real.";
            }
            else throw string( signature ) + " - Cannot convert " + toReadable( d ) + " to real.";
        }
        return( d );
    }
    Descriptor& toString( ThreadContext& ctx, Descriptor& d ) {
        static const char* signature( "void toString( Descriptor& )" );
        string str;
        auto type( typeCode( d ) );
        if (type != TypeString) { 
            if (type == TypeInteger) {
                str = to_string( integer( d ) );
            } else if (type == TypeReal) {
                str = to_string( real( d ) );
            } else throw string( signature ) + " - Cannot convert " + toReadable( d ) + " to string.";
            d = toStringDescriptor( ctx, str );
        }
        return( d );
    }
    Descriptor toStringDescriptor( ThreadContext& ctx, const string& source ) {
        auto n( source.size() );
        auto address( allocateString( ctx, n ) );
        strncpy( (char*)addressString( ctx, address ), source.c_str(), n );
        return StringDescriptor( address, n );
    }

    Descriptor& toNumeric( ThreadContext& ctx, Descriptor& descriptor ) {
        auto str( stringToCString( descriptor ) );
        size_t count = 0;
        auto integerValue( stoi( str, &count ) );
        if (count == str.size()) { descriptor = IntegerDescriptor( integerValue ); return descriptor; }
        auto realValue( stof( str, &count ) );
        if (count == str.size()) { descriptor = RealDescriptor( realValue ); return descriptor; }
        descriptor = NullDescriptor();
        return descriptor;
    }

    string stringToCString( ThreadContext& ctx, const Descriptor& d ) {
        // ToDo: Sanity check on string length and address
        static const char* signature( "string stringToCString( const Descriptor& )" );
        if (not isString( d )) throw string( signature ) + " - Cannot convert " + toReadable( d ) + " to C++ string";
        return string( reinterpret_cast<char*>( addressString( ctx, address( d ) ) ), type( d ) & TypeValueMask );
    }

    string toReadable( const Descriptor& d ) {
        string readable( "Descriptor< " );
        if (isLocalVariable( d )) readable += "L ";
        else if (isArgumentVariable( d )) readable += "A ";
        else if (isGlobalVariable( d )) readable += "G ";
        // if (t & Pointer) readable += "P ";
        // if (t & Trapped) readable += "T ";
        readable += typeToString( type( d ) );
        readable += " | ";
        if (typeCode( d ) == TypeString) readable += "\"" + stringToCString( d ) + "\""; else readable += to_string( word( d ) );
        readable += " >";
        return readable;
    }

} // namespace Language