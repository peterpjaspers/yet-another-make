#include "Functions.h"
#include "Instruction.h"
#include "ThreadContext.h"

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
            Dyadic( const Convert convert ) {
                static const char* signature( "Dyadic( const Convert convert )" );
                right = pop();
                left = pop();
                // 1 - If either is a variable, dereference it.
                // 2 - If either is an expression evaluate it. (ToDo:)
                if (isVariable( left )) left = dereference( left );
                if (isVariable( right )) right = dereference( right );
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
            Monadic( const Convert convert ) {
                static const char* signature( "Monadic( const Convert convert )" );
                operand = pop();
                if (isVariable( operand )) operand = dereference( operand );
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

    int32_t compareString( const Descriptor& left, const Descriptor& right ) {
        auto leftString( stringToCString( left ) );
        auto rightString( stringToCString( right ) );
        strcmp( leftString.c_str(), rightString.c_str() );
        return strcmp( leftString.c_str(), rightString.c_str() );
    }
    int32_t compareInteger( const Descriptor& left, const Descriptor& right ) {
        auto leftInteger( integer( left ) );
        auto rightInteger( integer( right ) );
        if ( leftInteger < rightInteger ) return( -1 );
        if ( rightInteger < leftInteger ) return( 1 );
        return( 0 );
    }
    int32_t compareReal( const Descriptor& left, const Descriptor& right ) {
        auto leftReal( real( left ) );
        auto rightReal( real( right ) );
        if ( leftReal < rightReal ) return( -1 );
        if ( rightReal < leftReal ) return( 1 );
        return( 0 );
    }
    int32_t compare() {
        static const char* signature( "int32_t compare()" );
        Dyadic ops( Convert::Arithmetic );
        if (typeCode( ops.left ) == TypeString) return compareString( ops.left, ops.right );
        else if (typeCode( ops.left ) == TypeInteger) return compareInteger( ops.left, ops.right );
        else if (typeCode( ops.left ) == TypeReal) return compareReal( ops.left, ops.right );
        else throw string( signature ) + " - Cannot compare " + typeToString(typeCode( ops.left )) + " with " + typeToString(typeCode( ops.right ));
    }
    Descriptor compare( int32_t success ) {
        auto cmp( compare() );
        if (cmp == success) return( IntegerDescriptor( 1 ) );
        return( NullDescriptor() );
    }
    Descriptor compare( int32_t success1, int32_t success2 ) {
        auto cmp( compare() );
        if ((cmp == success1) or (cmp == success2)) return( IntegerDescriptor( 1 ) );
        return( NullDescriptor() );
    }
    Descriptor FunAdd() {
        Dyadic ops( Convert::Arithmetic );
        if (typeCode( ops.left ) == TypeInteger) return( IntegerDescriptor( integer( ops.left ) + integer( ops.right ) ) );
        return( RealDescriptor( real( ops.left ) + real( ops.right ) ) );
    }
    Descriptor FunSub() {
        Dyadic ops( Convert::Arithmetic );
        if (typeCode( ops.left ) == TypeInteger) return( IntegerDescriptor( integer( ops.left ) - integer( ops.right ) ) );
        return( RealDescriptor( real( ops.left ) - real( ops.right ) ) );
    }
    Descriptor FunMul() {
        Dyadic ops( Convert::Arithmetic );
        if (typeCode( ops.left ) == TypeInteger) return( IntegerDescriptor( integer( ops.left ) * integer( ops.right ) ) );
        return( RealDescriptor( real( ops.left ) * real( ops.right ) ) );
    }
    Descriptor FunDiv() {
        Dyadic ops( Convert::Arithmetic );
        if (typeCode( ops.left ) == TypeInteger) return( IntegerDescriptor( integer( ops.left ) / integer( ops.right ) ) );
        return( RealDescriptor( real( ops.left ) / real( ops.right ) ) );
    }
    Descriptor FunEq() { return compare( 0 ); }
    Descriptor FunNeq() { return compare( -1, 1 ); }
    Descriptor FunLt() { return compare( -1 ); }
    Descriptor FunLteq() { return compare( -1, 0 ); }
    Descriptor FunGt() { return compare( 1 ); }
    Descriptor FunGteq() { return compare( 1, 0 ); }
    Descriptor FunCompare() { return IntegerDescriptor( compare() ); }
    Descriptor FunNot() {
        auto op( pop() );
        if (typeCode( op ) == TypeNull) return NullDescriptor();
        return( IntegerDescriptor( 1 ) );
    }
    Descriptor FunAnd() {
        Dyadic ops( Convert::None );
        if ((typeCode( ops.left ) == TypeNull) or (typeCode( ops.left ) == TypeNull)) return NullDescriptor();
        return( IntegerDescriptor( 1 ) );
    }
    Descriptor FunOr() {
        Dyadic ops( Convert::None );
        if ((typeCode( ops.left ) == TypeNull) and (typeCode( ops.left ) == TypeNull)) return NullDescriptor();
        return( IntegerDescriptor( 1 ) );
    }
    Descriptor FunShiftLeft() {
        Dyadic ops( Convert::Integer );
        return( IntegerDescriptor( integer( ops.left ) << integer( ops.right ) ) );
    }
    Descriptor FunShiftRight() {
        Dyadic ops( Convert::Integer );
        return( IntegerDescriptor( integer( ops.left ) >> integer( ops.right ) ) );
    }
    Descriptor FunBitwiseOr() {
        Dyadic ops( Convert::Integer );
        return( IntegerDescriptor( integer( ops.left ) | integer( ops.right ) ) );
    }
    Descriptor FunBitwiseXor() {
        Dyadic ops( Convert::Integer );
        return( IntegerDescriptor( integer( ops.left ) ^ integer( ops.right ) ) );
    }
    Descriptor FunBitwiseAnd() {
        Dyadic ops( Convert::Integer );
        return( IntegerDescriptor( integer( ops.left ) & integer( ops.right ) ) );
    }
    Descriptor FunBitwiseNegate() {
        Monadic op( Convert::Integer );
        return( IntegerDescriptor( ~integer( op.operand ) ) );
    }
    Descriptor FunInvert() {
        Monadic op( Convert::Arithmetic );
        if (typeCode(op.operand) == TypeInteger) return( IntegerDescriptor( -integer( op.operand ) ) );
        return( RealDescriptor( -real( op.operand ) ) );
    }
    Descriptor FunRemainder() {
        Dyadic ops( Convert::Integer );
        return( IntegerDescriptor( integer( ops.left ) % integer( ops.right ) ) );
    }

} // namespace Language