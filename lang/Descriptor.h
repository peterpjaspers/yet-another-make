#ifndef LANG_DESCRIPTOR_H
#define LANG_DESCRIPTOR_H

#include "Types.h"

namespace Language {

    
    // A Descriptor provides typed access to program data.
    // A descriptor is a 64-bit entity consisting of a 32-bit type and a 32-bit value.
    // The value field holds either the actual value (Word, Integer or Real) or an pointer to the value (Address).
    // The representation of the value field depends on the type of the descriptor.
    inline Type type( const Descriptor& d ) { return( d >> 32 ); }
    inline VariableType variableType( const Descriptor& d ) { return( VariableType(( type( d ) >> 29 ) & 0x3 ) ); }
    inline Word word( const Descriptor& d ) { return( static_cast<Word>( d & ((uint64_t( 1 ) << 32) - 1) ) ); }
    inline int32_t integer( const Descriptor& d ) { return( int32_t( word( d ) ) ); }
    inline float real( const Descriptor& d ) { return( std::bit_cast<float>( word( d ) ) ); }
    inline Address address( const Descriptor& d ) { return( Address( word( d ) ) ); }
    inline Descriptor descriptor( const Type t, const Word v ) { return( (uint64_t( t ) << 32) | uint64_t( v ) ); }
    inline bool isLocalVariable( const Descriptor& d ) { return( variableType( d ) == VariableType::Local ); }
    inline bool isArgumentVariable( const Descriptor& d ) { return( variableType( d ) == VariableType::Argument ); }
    inline bool isGlobalVariable( const Descriptor& d )  { return( variableType( d ) == VariableType::Global ); }
    inline bool isVariable( const Descriptor& d )  { return( variableType( d ) != VariableType::Undefined ); }
    inline bool isString( const Descriptor& d ) { return( not ( type( d ) & NS ) ); }
    inline Type typeCode( const Descriptor& d ) {
        if (isString( d )) return TypeString;
        return( type( d ) & TypeValueMask );
    }
    inline bool isNull( const Descriptor& d ) { return( typeCode( d ) == TypeNull ); }
    inline bool isInteger( const Descriptor& d ) { return( typeCode( d ) == TypeInteger ); }
    inline bool isReal( const Descriptor& d ) { return( typeCode( d ) == TypeReal ); }
    inline bool isAddress( const Descriptor& d ) { return( typeCode( d ) == TypeAddress ); }
    inline bool isProcedure( const Descriptor& d ) { return( typeCode( d ) == TypeProcedure ); }
    Descriptor dereference( const ThreadContext& context, const Descriptor& descriptor );
    Descriptor& toInteger( Descriptor& d );
    Descriptor& toReal( Descriptor& d );
    Descriptor& toString( Descriptor& d );
    // Convert String to numeric (either Integer or Real).
    // If String does not represent a numeric, converts to Null.
    Descriptor& toNumeric( Descriptor& descriptor );
    // Convert String Descriptor to C++ string
    std::string stringToCString( const Descriptor& d );
    // Convert Descriptor to human radable string
    std::string toReadable( const Descriptor& d );

} // namespace Language

#endif // LANG_DESCRIPTOR_H
