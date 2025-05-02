#include "Functions.h"
#include "Instruction.h"
#include "Memory.h"

#include <cmath>

// ToDo: Evaluate expressions if operand descriptor is an expression
// ToDo: Understand how to use C++ <=> operator for comparison (String, Integer, Real)

using namespace std;

namespace Language {

    namespace {

        // Convert operands to same arithmetic type: Integer or Real
        // If either is Real convert other to Real
        // If either is Integer convert other to Integer.
        // If either is String determine its numeric type (must be either Integer or Real) and convert other
        // If either is Null convert to Integer (zero) and convert other
        void toArithmetic( Descriptor& left, Descriptor& right ) {
            static const char* signature( "void toArithmetic( Discriptor&, Descriptor& )" );
            auto typeLeft( typeCode( left ) );
            auto typeRight( typeCode( right ) );
            if (typeLeft == TypeReal) toReal( right );
            else if (typeRight == TypeReal) toReal( left );
            else if (typeLeft == TypeInteger) toInteger( right );
            else if (typeRight == TypeInteger) toInteger( left );
            else if (typeLeft == TypeString) toArithmetic( toNumeric( left ), right );
            else if (typeRight == TypeString) toArithmetic( left, toNumeric( right ) );
            else if (typeLeft == TypeNull) toArithmetic( toInteger( left ), right );
            else if (typeRight == TypeNull) toArithmetic( left, toInteger( right ) );
            else throw string( signature ) + " - Incompatible operands : " + typeToString(typeCode( left )) + " and " + typeToString(typeCode( right ));
        }
        // Convert right operand to type of left
        void toLeft( Descriptor& left, Descriptor& right ) {
            static const char* signature( "void convertToLeft( Discriptor&, Descriptor& )" );
            // Convert right operand to type of left
            auto typeLeft( typeCode( left ) );
            if (typeLeft == TypeString) toString( right );
            else if (typeLeft == TypeInteger) toInteger( right );
            else if (typeCode( left ) == TypeReal) toReal( right );
            else throw string( signature ) + " - Incompatible operands : " + typeToString(typeCode( left )) + " and " + typeToString(typeCode( right ));
        }
        enum Convert {
            Integer,
            Real,
            String,
            Arithmetic,
            ToLeft,
            None
        };
        struct Dyadic {
            Descriptor left;
            Descriptor right;
            Dyadic( ThreadContext& context, const Convert convert ) {
                static const char* signature( "Dyadic( ThreadContext& context )" );
                right = pop( context );
                left = pop( context );
                // 1 - If either is a variable, dereference it.
                // 2 - If either is an expression evaluate it. (ToDo:)
                if (isVariable( left )) left = dereference( context, left );
                if (isVariable( right )) right = dereference( context, right );
                if (convert == Convert::Integer) { toInteger( left ); toInteger( right ); }
                else if (convert == Convert::Real) { toReal( left ); toReal( right ); }
                else if (convert == Convert::String) { toString( left ); toString( right ); }
                else if (convert == Convert::Arithmetic) toArithmetic( left, right );
                else if (convert == Convert::ToLeft) toLeft( left, right );
                else if (convert != Convert::None) throw string( signature ) + "- Invalid conversion request!";
            }
        };
        struct Monadic {
            Descriptor operand;
            Monadic( ThreadContext& context, const Convert convert ) {
                static const char* signature( "Monadic( ThreadContext& context )" );
                operand = pop( context );
                if (isVariable( operand )) operand = dereference( context, operand );
                if (convert == Convert::Integer) toInteger( operand );
                else if (convert == Convert::Real) toReal( operand );
                else if (convert == Convert::String) toString( operand );
                else if (convert == Convert::Arithmetic) {
                    if (isInteger( operand ) || isReal( operand )) return;
                    if (isString( operand )) {
                        toNumeric( operand );
                        if (isNull( operand )) throw string( signature ) + "- String is not numeric!";
                    }
                }
                else if (convert != Convert::None) throw string( signature ) + "- Invalid conversion request!";
            }
        };
    }

    int32_t compareString( ThreadContext& context, const Descriptor& left, const Descriptor& right ) {
        auto leftString( stringToCString( left ) );
        auto rightString( stringToCString( right ) );
        strcmp( leftString.c_str(), rightString.c_str() );
        return strcmp( leftString.c_str(), rightString.c_str() );
    }
    int32_t compareInteger( ThreadContext& context, const Descriptor& left, const Descriptor& right ) {
        auto leftInteger( integer( left ) );
        auto rightInteger( integer( right ) );
        if ( leftInteger < rightInteger ) return( -1 );
        if ( rightInteger < leftInteger ) return( 1 );
        return( 0 );
    }
    int32_t compareReal( ThreadContext& context, const Descriptor& left, const Descriptor& right ) {
        auto leftReal( real( left ) );
        auto rightReal( real( right ) );
        if ( leftReal < rightReal ) return( -1 );
        if ( rightReal < leftReal ) return( 1 );
        return( 0 );
    }
    int32_t compare( ThreadContext& context ) {
        static const char* signature( "int32_t compare( ThreadContext& context )" );
        Dyadic ops( context, Convert::Arithmetic );
        if (typeCode( ops.left ) == TypeString) return compareString( context, ops.left, ops.right );
        else if (typeCode( ops.left ) == TypeInteger) return compareInteger( context, ops.left, ops.right );
        else if (typeCode( ops.left ) == TypeReal) return compareReal( context, ops.left, ops.right );
        else throw string( signature ) + " - Cannot compare " + typeToString(typeCode( ops.left )) + " with " + typeToString(typeCode( ops.right ));
    }
    Descriptor compare( ThreadContext& context, int32_t success ) {
        auto cmp( compare( context ) );
        if (cmp == success) return( IntegerDescriptor( 1 ) );
        return( NullDescriptor() );
    }
    Descriptor compare( ThreadContext& context, int32_t success1, int32_t success2 ) {
        auto cmp( compare( context ) );
        if ((cmp == success1) or (cmp == success2)) return( IntegerDescriptor( 1 ) );
        return( NullDescriptor() );
    }
    Descriptor FunAdd( ThreadContext& context ) {
        Dyadic ops( context, Convert::Arithmetic );
        if (typeCode( ops.left ) == TypeInteger) return( IntegerDescriptor( integer( ops.left ) + integer( ops.right ) ) );
        return( RealDescriptor( real( ops.left ) + real( ops.right ) ) );
    }
    Descriptor FunSub( ThreadContext& context ) {
        Dyadic ops( context, Convert::Arithmetic );
        if (typeCode( ops.left ) == TypeInteger) return( IntegerDescriptor( integer( ops.left ) - integer( ops.right ) ) );
        return( RealDescriptor( real( ops.left ) - real( ops.right ) ) );
    }
    Descriptor FunMul( ThreadContext& context ) {
        Dyadic ops( context, Convert::Arithmetic );
        if (typeCode( ops.left ) == TypeInteger) return( IntegerDescriptor( integer( ops.left ) * integer( ops.right ) ) );
        return( RealDescriptor( real( ops.left ) * real( ops.right ) ) );
    }
    Descriptor FunDiv( ThreadContext& context ) {
        Dyadic ops( context, Convert::Arithmetic );
        if (typeCode( ops.left ) == TypeInteger) return( IntegerDescriptor( integer( ops.left ) / integer( ops.right ) ) );
        return( RealDescriptor( real( ops.left ) / real( ops.right ) ) );
    }
    Descriptor FunEq( ThreadContext& context ) { return compare( context, 0 ); }
    Descriptor FunNeq( ThreadContext& context ) { return compare( context, -1, 1 ); }
    Descriptor FunLt( ThreadContext& context ) { return compare( context, -1 ); }
    Descriptor FunLteq( ThreadContext& context ) { return compare( context, -1, 0 ); }
    Descriptor FunGt( ThreadContext& context ) { return compare( context, 1 ); }
    Descriptor FunGteq( ThreadContext& context ) { return compare( context, 1, 0 ); }
    Descriptor FunCompare( ThreadContext& context ) { return IntegerDescriptor( compare( context ) ); }
    Descriptor FunNot( ThreadContext& context ) {
        auto op( pop( context ) );
        if (typeCode( op ) == TypeNull) return NullDescriptor();
        return( IntegerDescriptor( 1 ) );
    }
    Descriptor FunAnd( ThreadContext& context ) {
        Dyadic ops( context, Convert::None );
        if ((typeCode( ops.left ) == TypeNull) or (typeCode( ops.left ) == TypeNull)) return NullDescriptor();
        return( IntegerDescriptor( 1 ) );
    }
    Descriptor FunOr( ThreadContext& context ) {
        Dyadic ops( context, Convert::None );
        if ((typeCode( ops.left ) == TypeNull) and (typeCode( ops.left ) == TypeNull)) return NullDescriptor();
        return( IntegerDescriptor( 1 ) );
    }
    Descriptor FunShiftLeft( ThreadContext& context ) {
        Dyadic ops( context, Convert::Integer );
        return( IntegerDescriptor( integer( ops.left ) << integer( ops.right ) ) );
    }
    Descriptor FunShiftRight( ThreadContext& context ) {
        Dyadic ops( context, Convert::Integer );
        return( IntegerDescriptor( integer( ops.left ) >> integer( ops.right ) ) );
    }
    Descriptor FunBitwiseOr( ThreadContext& context ) {
        Dyadic ops( context, Convert::Integer );
        return( IntegerDescriptor( integer( ops.left ) | integer( ops.right ) ) );
    }
    Descriptor FunBitwiseXor( ThreadContext& context ) {
        Dyadic ops( context, Convert::Integer );
        return( IntegerDescriptor( integer( ops.left ) ^ integer( ops.right ) ) );
    }
    Descriptor FunBitwiseAnd( ThreadContext& context ) {
        Dyadic ops( context, Convert::Integer );
        return( IntegerDescriptor( integer( ops.left ) & integer( ops.right ) ) );
    }
    Descriptor FunBitwiseNegate( ThreadContext& context ) {
        Monadic op( context, Convert::Integer );
        return( IntegerDescriptor( ~integer( op.operand ) ) );
    }
    Descriptor FunInvert( ThreadContext& context ) {
        Monadic op( context, Convert::Arithmetic );
        if (typeCode(op.operand) == TypeInteger) return( IntegerDescriptor( -integer( op.operand ) ) );
        return( RealDescriptor( -real( op.operand ) ) );
    }
    Descriptor FunRemainder( ThreadContext& context ) {
        Dyadic ops( context, Convert::Integer );
        return( IntegerDescriptor( integer( ops.left ) % integer( ops.right ) ) );
    }

} // namespace Language