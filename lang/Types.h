#ifndef LANG_TYPES_H
#define LANG_TYPES_H

#include <cstdint>
#include <string>
#include <bit>
#include <map>

#define _DEBUG_INTERPRETER = 1

// ToDo: Understand how to use UTF-8 coding for file names so that all strings can be char*

namespace Language {

    typedef uint64_t Descriptor;
    typedef uint32_t Type;
    typedef uint32_t Word;
    typedef uint32_t Integer;
    typedef float Real;
    typedef uint32_t Address;
    typedef uint32_t Offset;
    typedef uint8_t* PageAddress;


    // Value types
    static const Type TypeNull( 0 );
    static const Type TypeInteger( 1 );
    static const Type TypeReal( 2 );
    static const Type TypeString( 3 );
    static const Type TypeProcedure( 4 );
    static const Type TypeList( 5 );
    static const Type TypeSet( 6 );
    static const Type TypeMap( 7 );
    static const Type TypeRecord( 8 );
    static const Type TypeAddress( 9 );
    static const Type TypeIntrinsic( 10 );
    std::string typeToString( const Type type );

    // Desriptor access
    enum VariableType {
        Undefined   = 0,
        Local       = 1,
        Argument    = 2,
        Global      = 3
    };

    static const Type NS( 1 << 31 );                        // Not string
    static const Type LV( VariableType::Local << 29 );      // Local Variable
    static const Type AV( VariableType::Argument << 29 );   // Argument Variable
    static const Type GV( VariableType::Global << 29 );     // Global Variable
    static const Type Pointer( 1 << 28 );
    static const Type Trapped( 1 << 27 );
    static const Type TypeValueMask( (1 << 27) - 1 );

    // Descriptors types
    static const Word NullTypeWord( NS | TypeNull );
    static const Word IntegerTypeWord( NS | TypeInteger );
    static const Word RealTypeWord( NS | TypeReal );
    static const Word StringTypeWord( 0 );
    static const Word AddressTypeWord( NS | TypeAddress );
    static const Word LocalVariableTypeWord( NS | LV | TypeAddress );
    static const Word ArgumentVariableTypeWord( NS | AV | TypeAddress );
    static const Word GlobalVariableTypeWord( NS | GV | TypeAddress );
    static const Word ProcedureTypeWord( NS | Pointer | TypeProcedure );
    static const Word IntrinsicTypeWord( NS | TypeIntrinsic );
    static const Word ListTypeWord( NS | Pointer | TypeList );
    static const Word SetTypeWord( NS | Pointer | TypeSet );
    static const Word MapTypeWord( NS | Pointer | TypeMap );
    static const Word RecordTypeWord( NS | Pointer | TypeRecord );

    inline Descriptor NullDescriptor() { return( (Descriptor)NullTypeWord << 32 ); }
    inline Descriptor IntegerDescriptor( Word value ) { return( ((Descriptor)IntegerTypeWord << 32) | value ); }
    inline Descriptor RealDescriptor( float value ) { return( ((Descriptor)RealTypeWord << 32) | std::bit_cast<Word>(value) ); }
    inline Descriptor StringDescriptor( Address address, Word size ) { return( (((Descriptor)StringTypeWord | size) << 32) | address ); }
    inline Descriptor LocalVariableDescriptor( Word offset ) { return( ((Descriptor)LocalVariableTypeWord << 32) | offset ); }
    inline Descriptor ArgumentVariableDescriptor( Address address ) { return( ((Descriptor)ArgumentVariableTypeWord << 32) | address ); }
    inline Descriptor GlobalVariableDescriptor( Address address ) { return( ((Descriptor)GlobalVariableTypeWord << 32) | address ); }
    inline Descriptor ProcedureDescriptor( Address address ) { return( ((Descriptor)ProcedureTypeWord << 32) | address ); }
    inline Descriptor IntrinsicDescriptor( Word value ) { return( ((Descriptor)IntrinsicTypeWord << 32) | value ); }
    inline Descriptor AddressDescriptor( Address address ) { return( ((Descriptor)AddressTypeWord << 32) | address ); }
    inline Descriptor BuildDescriptor( Type type, Word value ) { return( ((Descriptor)type << 32) | value ); }

} // namespace Language

#endif // LANG_TYPES_H
