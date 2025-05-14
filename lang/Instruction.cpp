#include "Instruction.h"
#include "ThreadContext.h"
#include "Functions.h"
#include "Monitor.h"

#include <iostream>

// ToDo: Include symbol name when outputing monitoring info

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
    inline Word fetchWordOperand() {
        auto& ctx( context() );
        auto operand( loadWordOperand( ctx.pc ) );
        ctx.pc += 4;
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
    inline Descriptor fetchDescriptorOperand() {
        auto& ctx( context() );
        auto operand( loadDescriptorOperand( ctx.pc ) );
        ctx.pc += 8;
        return operand;
    }
    ostream& monitorInstruction( OpCode code ) {
        auto& ctx( constContext() );
        ostream& record = monitorRecord() << setw( 4 ) << ctx.pc << " - " << opCodeTable[ code ].name;
        if (opCodeTable[ code ].operand == 1) {
            auto operand( loadWordOperand( ctx.pc + sizeof( OpCode ) ) );
            if (code == OpPushReal) record << "( " << bit_cast<float>( operand ) << " )";
            else record << "( " << operand << " )";
        } else if (opCodeTable[ code ].operand == 2) {
            record << "( " << toReadable( loadDescriptorOperand( ctx.pc + sizeof( OpCode ) ) ) << " )";
        }
        return record;
    }
    void push( const Descriptor& descriptor ) {
        auto address( addressStack() );
        if (monitor( DebugAspects::StackOperations )) monitorRecord() << "push(" << toReadable( descriptor ) << " )" << record<char>;
        *address = descriptor;
        context().sp += sizeof( Descriptor );
    }
    Descriptor pop() {
        auto d( *addressStack( sizeof( Descriptor ) ) );
        if (monitor( DebugAspects::StackOperations )) monitorRecord() << "pop()" << record<char>;
        #ifdef _DEBUG_INTERPRETER
            static const char* signature( "Descriptor pop()" );
            if (constContext().sp < sizeof( Descriptor )) throw string( signature ) + " - Stack underflow";
        #endif
        context().sp -= sizeof( Descriptor );
        return d;
    }
    inline void jump() {
        Address transfer( fetchWordOperand() );
        context().pc = transfer;
    }
    inline void conditionalJump() {
        Address transfer( fetchWordOperand() );
        if (isNull( pop() )) context().pc = transfer;
    }
    inline void exitProgram() {
        auto d( pop() );
        toInteger( d );
        exit( integer( d ) );
    }
    // Mark start of argument expression evaluation
    inline void arguments() {
        auto& ctx( context() );
        push( AddressDescriptor( ctx.ep ) );
        ctx.ep = ctx.sp;
    }
    inline void procedureCall() {
        static const char* signature( "void procedureCall()" );
        // Pick-up (procedure) descriptor, the procedure descriptor and saved expression pointer descriptor
        // are located under the current expression pointer (ep).
        auto& ctx( context() );
        Descriptor& transfer( *reinterpret_cast<Descriptor*>( addressMemory( ctx.stack, ctx.ep - (2 * sizeof( Descriptor )) ) ) );
        // The descriptor may be a file or a string in which case it must translated (not implemented yet).
        if (!isProcedure( transfer )) throw string( signature ) + " - Procedures call address invalid";
        if (monitor( DebugAspects::ProcedureCall )) monitorRecord() << "Procedure call " << transfer << record<char>;
        push( AddressDescriptor( ctx.pc ) );
        push( AddressDescriptor( ctx.fp ) );
        ctx.pc = address( transfer );
        ctx.fp = ctx.sp;
        ctx.ap = ctx.ep;
    }
    inline void procedureReturn() {
        static const char* signature( "void procedureReturn()" );
        auto& ctx( context() );
        // Replace procedure call descriptor with procedure result
        Descriptor& result( *reinterpret_cast<Descriptor*>( addressMemory( ctx.stack, (ctx.ap - (2 * sizeof( Descriptor )) ) ) ) );
        result = pop();
        #ifdef _DEBUG_INTERPRETER
            if (monitor( DebugAspects::ProcedureCall )) monitorRecord() << "Procedure return " << record<char>;
            ctx.sp = ctx.fp;
            auto fp( pop() );
            auto pc( pop() );
            if (!isAddress( pc ) or !isAddress( fp )) throw string( signature ) + " - Corrupt stack";
            ctx.pc = address( pc );
            ctx.fp = address( fp );
            ctx.sp = ctx.ap;
            auto ap( pop() );
            if (!isAddress( ap )) throw string( signature ) + " - Corrupt stack";
            ctx.ep = ( ctx.ap = address( ap ) );
        #elif
            ctx.sp = ctx.fp;
            ctx.fp( address( pop() ) );
            ctx.pc( address( pop() ) );
            ctx.sp = ctx.ap;
            ctx.ap( address( pop() ) );
        #endif
    }
    inline void locals() {
        Word locals( fetchWordOperand() );
        context().sp += locals;
    }
    inline void loadArgument() {
        #ifdef _DEBUG_INTERPRETER
            static const char* signature( "void loadArgument( ThreadContext& context )" );
            Word offset( fetchWordOperand() );
            auto& ctx( context() );
            auto range( ctx.fp - ctx.ap );
            if (range <= offset) throw string( signature ) + " - Argument out of range";
            push( *addressArgument( offset ) );
        #else
            push( *addressArgument( fetchWordOperand() ) );
        #endif
    }
    inline void loadLocal() {
        push( *addressLocal( fetchWordOperand() ) );
    }
    inline void loadGlobal() {
        push( *addressGlobal( fetchWordOperand() ) );
    }
    inline void storeArgument() {
        #ifdef _DEBUG_INTERPRETER
            static const char* signature( "void storeArgument( ThreadContext& context )" );
            Word offset( fetchWordOperand() );
            auto& ctx( context() );
            auto range( ctx.fp - ctx.ap );
            if (range <= offset) throw string( signature ) + " - Argument out of range";
            *addressArgument( offset ) = pop();
        #else
            *addressArgument( fetchWordOperand() ) = pop();
        #endif
    }
    inline void storeLocal() {
        *addressLocal( fetchWordOperand() ) = pop();
    }
    inline void storeGlobal() {
        *addressGlobal( fetchWordOperand() ) = pop();
    }
    inline void assign() {
        static const char* signature( "void assign( ThreadContext& context )" );
        auto rhs( pop() );
        auto lhs( addressStack( sizeof( Descriptor ) ) );
        if (isLocalVariable( *lhs )) {
            *addressLocal( address( *lhs ) ) = rhs;
        } else if (isArgumentVariable( *lhs )) {
            *addressArgument( address( *lhs ) ) = rhs;
        } else if (isGlobalVariable( *lhs )) {
            *addressGlobal( address( *lhs ) ) = rhs;
        } else {
            throw string( signature ) + " - Corrupt stack"; 
        }
        *lhs = rhs;
    }
    inline void duplicate() {
        auto value( addressStack( sizeof( Descriptor ) ) );
        *(value + 1) = *value;
        context().sp += sizeof( Descriptor );
    }
    // Output expression value to console
    void output() {
        auto value( pop() );
        if (isVariable( value )) value = dereference( value );
        auto str( stringToCString( toString( value ) ) );
        cout << str << endl;
    }
    bool executeInstruction() {
        static const char* signature( "void executeInstruction()" );
        auto& ctx( context() );
        auto address( addressProgram( ctx.pc ) );
        OpCode code( *address );
        if (monitor( DebugAspects::InstructionExecution )) monitorInstruction( code ) << record<char>;
        ctx.pc += 1;
        switch ( code ) {
            case OpPushNull : push( NullDescriptor() ); break;
            case OpPushInteger : push( IntegerDescriptor( fetchWordOperand() ) ); break;
            case OpPushReal : push( RealDescriptor( bit_cast<float>(fetchWordOperand()) ) ); break;
            case OpPushDescriptor : push( fetchDescriptorOperand() ); break;
            case OpPop : pop(); break;
            case OpAdd : push( FunAdd() ); break;
            case OpSub : push( FunSub() ); break;
            case OpMul : push( FunMul() ); break;
            case OpDiv : push( FunDiv() ); break;
            case OpEq : push( FunEq() ); break;
            case OpNeq : push( FunNeq() ); break;
            case OpLt : push( FunLt() ); break;
            case OpLteq : push( FunLteq() ); break;
            case OpGt : push( FunGt() ); break;
            case OpGteq : push( FunGteq() ); break;
            case OpCompare : push( FunCompare() ); break;
            case OpNot : push( FunNot() ); break;
            case OpAnd : push( FunAnd() ); break;
            case OpOr : push( FunOr() ); break;
            case OpJump : jump(); break;
            case OpConditionalJump : conditionalJump(); break;
            case OpArguments : arguments(); break;
            case OpProcedureCall : procedureCall(); break;
            case OpReturn : procedureReturn(); break;
            case OpLocals : locals(); break;
            case OpLoadArgument : loadArgument(); break;
            case OpLoadLocal : loadLocal(); break;
            case OpLoadGlobal : loadGlobal(); break;
            case OpStoreArgument : storeArgument(); break;
            case OpStoreLocal : storeLocal(); break;
            case OpStoreGlobal : storeGlobal(); break;
            case OpAssign : assign(); break;
            case OpDup : duplicate(); break;
            case OpShiftLeft : FunShiftLeft(); break;
            case OpShiftRight : FunShiftRight(); break;
            case OpBitOr : FunBitwiseOr(); break;
            case OpBitXor : FunBitwiseXor(); break;
            case OpBitAnd : FunBitwiseAnd(); break;
            case OpNegate : FunBitwiseNegate(); break;
            case OpInvert : FunInvert(); break;
            case OpRem : FunRemainder(); break;
            case OpFail : break;
            case OpMark : break;
            case OpUnmark : break;
            case OpOutput : output(); break;
            case OpExit : return false;
            default : throw string( signature ) + " - Invalid op-code " + to_string( code );
        }
        return true;
    }
    // ToDo: Provides means of passing arguments to program
    int run( const Address start ) {
        try {
            auto& ctx( context() );
            ctx.pc = start;
            while (executeInstruction()) {};
            // Pick-up return value left behind on the stack, if any
            if (sizeof(Descriptor) < ctx.sp) {
                auto value( pop() );
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
