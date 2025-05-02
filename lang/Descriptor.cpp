#include "Descriptor.h"
#include "Memory.h"

using namespace std;

namespace Language {

    Descriptor dereference( const ThreadContext& context, const Descriptor& descriptor ) {
        static const char* signature( "Descriptor dereference( const ThreadContext& context, const Descriptor& descriptor )" );
        auto adr( address( descriptor ) );
        if (isLocalVariable( descriptor)) return *addressLocal( context, adr );
        if (isArgumentVariable( descriptor)) return *addressArgument( context, adr );
        if (isGlobalVariable( descriptor)) return *addressGlobal( adr );
        throw string( signature ) + " - Dereferencing unknown variable type";
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
                else if (typeCode( d ) != TypeInteger) throw string( signature ) + " - Cannot convert String descriptor to integer.";
            }
            else throw string( signature ) + " - Cannot convert " + typeToString( type ) + " descriptor to integer.";
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
                else if (typeCode( d ) != TypeReal) throw string( signature ) + " - Cannot convert String descriptor to integer.";
            }
            else throw string( signature ) + " - Cannot convert " + typeToString( type ) + " descriptor to real.";
        }
        return( d );
    }
    Descriptor& toString( Descriptor& d ) {
        static const char* signature( "void toString( Descriptor& )" );
        string str;
        auto type( typeCode( d ) );
        if (type != TypeString) { 
            if (type == TypeInteger) {
                str = to_string( integer( d ) );
            } else if (type == TypeReal) {
                str = to_string( real( d ) );
            } else throw string( signature ) + " - Cannot convert " + typeToString( type ) + " descriptor to string.";
            Word n( str.size() );
            Address a( allocateString( n ) );
            strncpy( reinterpret_cast<char *>( addressString( a ) ), str.c_str(), n );
            d = StringDescriptor( a, n );
        }
        return( d );
    }
    Descriptor& toNumeric( Descriptor& descriptor ) {
        auto str( stringToCString( descriptor ) );
        size_t count = 0;
        auto integerValue( stoi( str, &count ) );
        if (count == str.size()) { descriptor = IntegerDescriptor( integerValue ); return descriptor; }
        auto realValue( stof( str, &count ) );
        if (count == str.size()) { descriptor = RealDescriptor( realValue ); return descriptor; }
        descriptor = NullDescriptor();
        return descriptor;
    }

    string stringToCString( const Descriptor& d ) {
        static const char* signature( "string stringToCString( const Descriptor& )" );
        if (not isString( d )) throw string( signature ) + " - Cannot convert " + typeToString( type( d ) ) + " descriptor to C++ string";
        return string( reinterpret_cast<char*>( addressString( address( d ) ) ), type( d ) & TypeValueMask );
    }

    // ToDo: Format as function of descriptor type (string, value, address...)
    string toReadable( const Descriptor& d ) {
        string readable( "Descriptor< " );
        auto t( type( d ) );
        auto w( word( d ) );
        if (isLocalVariable( d )) readable += "L ";
        else if (isArgumentVariable( d )) readable += "A ";
        else if (isGlobalVariable( d )) readable += "G ";
        // if (t & Pointer) readable += "P ";
        // if (t & Trapped) readable += "T ";
        readable += typeToString( t );
        readable += " | ";
        readable += to_string( w );
        readable += " >";
        return readable;
    }

} // namespace Language