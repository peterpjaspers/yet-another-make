#include "Instruction.h"
#include "Memory.h"
#include "Functions.h"
#include "Monitor.h"

#include <iostream>

// ToDo: Include symbol name when outputing monitoring info
// ToDo: Check stack pointer at end of each instruction, looks pushes and pops are not matched

using namespace std;

namespace Language {

    // ToDo: Account for machine endianship

    void storeWordOperand( const Address pc, const Word w ) {
        if (4 <= pageRemainder( pc )) {
            auto address( addressProgram( pc ) );
            *reinterpret_cast<Word*>( address ) = w;
        } else {
           *addressProgram( pc + 0 ) = ((w >>  0) & 0xFF);
           *addressProgram( pc + 1 ) = ((w >>  8) & 0xFF);
           *addressProgram( pc + 2 ) = ((w >> 16) & 0xFF);
           *addressProgram( pc + 3 ) = ((w >> 24) & 0xFF);
        }
    }
    Word loadWordOperand( const Address pc ) {
        if (4 <= pageRemainder( pc )) {
            auto address( addressProgram( pc ) );
            return( *reinterpret_cast<Word*>(address) );
        } else {
            Word byte0( *addressProgram( pc + 0 ) );
            Word byte1( *addressProgram( pc + 1 ) );
            Word byte2( *addressProgram( pc + 2 ) );
            Word byte3( *addressProgram( pc + 3 ) );
            return( byte3 << 24 || byte2 << 16 || byte1 << 8 || byte0 );
        }
    }
    inline Word fetchWordOperand( ThreadContext& context ) {
        auto operand( loadWordOperand( context.pc ) );
        context.pc += 4;
        return operand;
    }
    void storeDescriptorOperand( const Address pc, const Descriptor d ) {
        if (8 <= pageRemainder( pc )) {
            auto address( addressProgram( pc ) );
            *reinterpret_cast<Descriptor*>( address ) = d;
        } else {
           *addressProgram( pc + 0 ) = ((d >>  0) & 0xFF);
           *addressProgram( pc + 1 ) = ((d >>  8) & 0xFF);
           *addressProgram( pc + 2 ) = ((d >> 16) & 0xFF);
           *addressProgram( pc + 3 ) = ((d >> 24) & 0xFF);
           *addressProgram( pc + 4 ) = ((d >> 32) & 0xFF);
           *addressProgram( pc + 5 ) = ((d >> 40) & 0xFF);
           *addressProgram( pc + 6 ) = ((d >> 48) & 0xFF);
           *addressProgram( pc + 7 ) = ((d >> 56) & 0xFF);
        }
    }
    Descriptor loadDescriptorOperand( const Address pc ) {
        if (8 <= pageRemainder( pc )) {
            auto address( addressProgram( pc ) );
            return( *reinterpret_cast<Descriptor*>(address) );
        } else {
            Descriptor byte0( *addressProgram( pc + 0 ) );
            Descriptor byte1( *addressProgram( pc + 1 ) );
            Descriptor byte2( *addressProgram( pc + 2 ) );
            Descriptor byte3( *addressProgram( pc + 3 ) );
            Descriptor byte4( *addressProgram( pc + 4 ) );
            Descriptor byte5( *addressProgram( pc + 5 ) );
            Descriptor byte6( *addressProgram( pc + 6 ) );
            Descriptor byte7( *addressProgram( pc + 7 ) );
            return( byte7 << 56 | byte6 << 48 | byte5 << 40 | byte4 << 32 | byte3 << 24 | byte2 << 16 | byte1 << 8 | byte0 );
        }
    }
    inline Descriptor fetchDescriptorOperand( ThreadContext& context ) {
        auto operand( loadDescriptorOperand( context.pc ) );
        context.pc += 8;
        return operand;
    }
    ostream& monitorInstruction( ThreadContext& context, OpCode code ) {
        ostream& record = monitorRecord() << setw( 4 ) << (context.pc - 1) << " - " << opCodeTable[ code ].name;
        if (opCodeTable[ code ].operand == 1) {
            auto operand( loadWordOperand( context.pc ) );
            if (code == OpPushReal) record << "( " << bit_cast<float>( operand ) << " )";
            else record << "( " << operand << " )";
        } else if (opCodeTable[ code ].operand == 2) { record << "( " << toReadable( loadDescriptorOperand( context.pc ) ) << " )"; }
        return record;
    }
    void push( ThreadContext& context, const Descriptor& descriptor ) {
        auto address( addressStack( context ) );
        if (monitor( DebugAspects::StackOperations )) monitorRecord() << "push(" << toReadable( descriptor ) << " )" << record<char>;
        *address = descriptor;
        context.sp += sizeof( Descriptor );
    }
    Descriptor pop( ThreadContext& context ) {
        auto d( *addressStack( context, sizeof( Descriptor ) ) );
        if (monitor( DebugAspects::StackOperations )) monitorRecord() << "pop()" << record<char>;
        #ifdef _DEBUG_INTERPRETER
            static const char* signature( "Descriptor pop( ThreadContext& context )" );
            if (context.sp < sizeof( Descriptor )) throw string( signature ) + " - Stack underflow";
        #endif
        context.sp -= sizeof( Descriptor );
        return d;
    }
    inline void jump( ThreadContext& context ) {
        Address transfer( fetchWordOperand( context ) );
        context.pc = transfer;
    }
    inline void conditionalJump( ThreadContext& context ) {
        Address transfer( fetchWordOperand( context ) );
        if (isNull( pop( context ) )) context.pc = transfer;
    }
    inline void exitProgram( ThreadContext& context ) {
        auto d( pop( context ) );
        toInteger( d );
        exit( integer( d ) );
    }
    // Mark start of argument expression evaluation
    inline void arguments( ThreadContext& context ) {
        push( context, AddressDescriptor( context.ep ) );
        context.ep = context.sp;
    }
    inline void procedureCall( ThreadContext& context ) {
        static const char* signature( "void procedureCall( ThreadContext& context )" );
        // Pick (procedure) descriptor, this is on the stack just below the argument pointer (ap)
        // The descriptor may be a file or a string in which case it must translated.
        Descriptor& transfer( *reinterpret_cast<Descriptor*>( addressMemory( context.stack, (context.ep - 16 ) ) ) );
        if (monitor( DebugAspects::ProcedureCall )) monitorRecord() << "Procedure call " << transfer << record<char>;
        if (!isProcedure( transfer )) throw string( signature ) + " - Procedures call address invalid";
        push( context, AddressDescriptor( context.pc ) );
        push( context, AddressDescriptor( context.fp ) );
        context.pc = address( transfer );
        context.fp = context.sp;
        context.ap = context.ep;
    }
    inline void procedureReturn( ThreadContext& context ) {
        static const char* signature( "void procedureReturn( ThreadContext& )" );
        // Replace procedure call descriptor with procedure result
        Descriptor& result( *reinterpret_cast<Descriptor*>( addressMemory( context.stack, (context.ap - 16 ) ) ) );
        result = pop( context );
        #ifdef _DEBUG_INTERPRETER
            // Adjust stack-pointer with local variable count
            if (monitor( DebugAspects::ProcedureCall )) monitorRecord() << "Procedure return " << record<char>;
            Descriptor& locals( *reinterpret_cast<Descriptor*>( addressMemory( context.stack, context.fp ) ) );
            if (!isInteger( locals )) throw string( signature ) + " - Corrupt stack";
            // Restore frame-pointer, stack-pointer, argument-pointer and program-counter
            context.sp -= word( locals + 8 );
            auto fp( pop( context ) );
            auto pc( pop( context ) );
            if (!isAddress( pc ) or !isAddress( fp )) throw string( signature ) + " - Corrupt stack";
            context.pc = address( pc );
            context.fp = address( fp );
            context.sp = context.ap;
            auto ap( pop( context ) );
            if (!isAddress( ap )) throw string( signature ) + " - Corrupt stack";
            context.ep = ( context.ap = address( ap ) );
        #elif
            context.fp( address( pop( context ) ) );
            context.pc( address( pop( context ) ) );
            context.sp = context.ap;
            context.ap( address( pop( context ) ) );
        #endif
    }
    inline void locals( ThreadContext& context ) {
        Word locals( fetchWordOperand( context ) );
        push( context, IntegerDescriptor( locals ) );
        // Initialize local variables to null
        auto localNulls( addressStack( context ) );
        for (int i = 0; i < (locals / sizeof( Descriptor )); ++i) { *localNulls++ = NullDescriptor(); }
        context.sp += locals;
    }
    inline void loadArgument( ThreadContext& context ) {
        #ifdef _DEBUG_INTERPRETER
            static const char* signature( "void loadArgument( ThreadContext& context )" );
            Word offset( fetchWordOperand( context ) );
            auto range( context.fp - context.ap );
            if (range <= offset) throw string( signature ) + " - Argument out of range";
            push( context, *addressArgument( context, offset ) );
        #else
            push( context, *addressArgument( context, fetchWordOperand( context ) ) );
        #endif
    }
    inline void loadLocal( ThreadContext& context ) {
        push( context, *addressLocal( context, fetchWordOperand( context ) ) );
    }
    inline void loadGlobal( ThreadContext& context ) {
        push( context, *addressGlobal( fetchWordOperand( context ) ) );
    }
    inline void storeArgument( ThreadContext& context ) {
        #ifdef _DEBUG_INTERPRETER
            static const char* signature( "void storeArgument( ThreadContext& context )" );
            Word offset( fetchWordOperand( context ) );
            auto range( context.fp - context.ap );
            if (range <= offset) throw string( signature ) + " - Argument out of range";
            *addressArgument( context, offset ) = pop( context );
        #else
            *addressArgument( context, fetchWordOperand( context ) ) = pop( context );
        #endif
    }
    inline void storeLocal( ThreadContext& context ) {
        *addressLocal( context, fetchWordOperand( context ) ) = pop( context );
    }
    inline void storeGlobal( ThreadContext& context ) {
        *addressGlobal( fetchWordOperand( context ) ) = pop( context );
    }
    inline void assign( ThreadContext& context ) {
        static const char* signature( "void assign( ThreadContext& context )" );
        auto rhs( pop( context ) );
        auto lhs( addressStack( context, sizeof( Descriptor ) ) );
        if (isLocalVariable( *lhs )) {
            *addressLocal( context, address( *lhs ) ) = rhs;
        } else if (isArgumentVariable( *lhs )) {
            *addressArgument( context, address( *lhs ) ) = rhs;
        } else if (isGlobalVariable( *lhs )) {
            *addressGlobal( address( *lhs ) ) = rhs;
        } else {
            throw string( signature ) + " - Corrupt stack"; 
        }
        *lhs = rhs;
    }
    inline void duplicate( ThreadContext& context ) {
        auto value( addressStack( context, sizeof( Descriptor ) ) );
        *(value + 1) = *value;
        context.sp += sizeof( Descriptor );
    }
    // Output expression value to console
    void output( ThreadContext& context ) {
        auto value( pop( context ) );
        if (isVariable( value )) value = dereference( context, value );
        auto str( stringToCString( toString( value ) ) );
        cout << str << endl;
    }
    bool executeInstruction( ThreadContext& context ) {
        static const char* signature( "void executeInstruction( ThreadContext& context )" );
        auto address( addressProgram( context.pc++ ) );
        OpCode code( *address );
        if (monitor( DebugAspects::InstructionExecution )) monitorInstruction( context, code ) << record<char>;
        switch ( code ) {
            case OpPushNull : push( context, NullDescriptor() ); break;
            case OpPushInteger : push( context, IntegerDescriptor( fetchWordOperand( context ) ) ); break;
            case OpPushReal : push( context, RealDescriptor( bit_cast<float>(fetchWordOperand( context )) ) ); break;
            case OpPushDescriptor : push( context, fetchDescriptorOperand( context ) ); break;
            case OpPop : pop( context ); break;
            case OpAdd : push( context, FunAdd( context ) ); break;
            case OpSub : push( context, FunSub( context ) ); break;
            case OpMul : push( context, FunMul( context ) ); break;
            case OpDiv : push( context, FunDiv( context ) ); break;
            case OpEq : push( context, FunEq( context ) ); break;
            case OpNeq : push( context, FunNeq( context ) ); break;
            case OpLt : push( context, FunLt( context ) ); break;
            case OpLteq : push( context, FunLteq( context ) ); break;
            case OpGt : push( context, FunGt( context ) ); break;
            case OpGteq : push( context, FunGteq( context ) ); break;
            case OpCompare : push( context, FunCompare( context ) ); break;
            case OpNot : push( context, FunNot( context ) ); break;
            case OpAnd : push( context, FunAnd( context ) ); break;
            case OpOr : push( context, FunOr( context ) ); break;
            case OpJump : jump( context ); break;
            case OpConditionalJump : conditionalJump( context ); break;
            case OpArguments : arguments( context ); break;
            case OpProcedureCall : procedureCall( context ); break;
            case OpReturn : procedureReturn( context ); break;
            case OpLocals : locals( context ); break;
            case OpLoadArgument : loadArgument( context ); break;
            case OpLoadLocal : loadLocal( context ); break;
            case OpLoadGlobal : loadGlobal( context ); break;
            case OpStoreArgument : storeArgument( context ); break;
            case OpStoreLocal : storeLocal( context ); break;
            case OpStoreGlobal : storeGlobal( context ); break;
            case OpAssign : assign( context ); break;
            case OpDup : duplicate( context ); break;
            case OpShiftLeft : FunShiftLeft( context ); break;
            case OpShiftRight : FunShiftRight( context ); break;
            case OpBitOr : FunBitwiseOr( context ); break;
            case OpBitXor : FunBitwiseXor( context ); break;
            case OpBitAnd : FunBitwiseAnd( context ); break;
            case OpNegate : FunBitwiseNegate( context ); break;
            case OpInvert : FunInvert( context ); break;
            case OpRem : FunRemainder( context ); break;
            case OpFail : break;
            case OpMark : break;
            case OpUnmark : break;
            case OpOutput : output( context ); break;
            case OpExit : return false;
            default : throw string( signature ) + " - Invalid op-code " + to_string( code );
        }
        return true;
    }
    // ToDo: Provides means of passing arguments to program
    int run( const Address start ) {
        try {
            ThreadContext context;
            context.pc = start;
            while (executeInstruction( context )) {};
            // Pick-up return value left behind on the stack, if any
            if (sizeof(Descriptor) < context.sp) {
                auto value( pop( context ) );
                return word( toInteger( value ) );
            } else return( 0 );
        }
        catch (string message) { cerr << message << endl; }
        catch (...) { cerr << "Exception thrown..." << endl; }
        return( -1 );
    }
    void storeInstruction( const OpCode code ) {
        if (monitor( DebugAspects::GeneratedCode )) {
            monitorRecord() << setw( 20 ) << "-> " << toReadable( code ) << record<char>;
        }
        auto address( addressProgram( allocateProgram( sizeof( OpCode ) ) ) );
        *address = code;
    }
    void storeInstruction( const OpCode code, const Word operand ) {
        if (monitor( DebugAspects::GeneratedCode )) {
            string opString;
            if (code == OpPushReal) opString = to_string( bit_cast<float>( operand ) );
            else opString = to_string( operand );
            monitorRecord() << setw( 20 ) << "-> " << toReadable( code ) << "[ " << opString << " ]" << record<char>;
        }
        auto pc( allocateProgram( sizeof( OpCode ) + sizeof( Word ) ) );
        *addressProgram( pc ) = code;
        storeWordOperand( ( pc + sizeof( OpCode ) ), operand );
    }
    void storeInstruction( const OpCode code, const Descriptor operand ) {
        if (monitor( DebugAspects::GeneratedCode )) {
            monitorRecord() << setw( 20 ) << "-> " << toReadable( code ) << "[ " << toReadable( operand ) << " ]"<< record<char>;
        }
        auto pc( allocateProgram( sizeof( OpCode ) + sizeof( Descriptor ) ) );
        *addressProgram( pc ) = code;
        storeDescriptorOperand( ( pc + sizeof( OpCode ) ), operand );
    }

    string toReadable( const OpCode code ) { return opCodeTable[ code ].name; }

    void printProgram( const std::filesystem::path file ) {
        LogFile stream( file, false, false );
        printProgram( stream );
    }
    void printProgram( LogFile& stream ) {
        Address pc( 0 );
        Address extent( allocateProgram( 0 ) );
        while (pc < extent) {
            auto address( addressProgram( pc ) );
            OpCode code( *address );
            ostream& instruction = stream() << setw( 4 ) << pc << " : " << toReadable( code );
            pc += 1;
            if (opCodeTable[ code ].operand == 1) {
                auto operand( loadWordOperand( pc ) );
                if (code == OpPushReal) instruction << "( " << bit_cast<float>( operand ) << " )";
                else instruction << "( " << operand << " )";
                pc += 4;
            } else if (opCodeTable[ code ].operand == 2) {
                instruction << "( " << toReadable( loadDescriptorOperand( pc ) ) << " )";
                pc += 8;
            }
            instruction << record<char>;
        }
    }

} // namespace Language
