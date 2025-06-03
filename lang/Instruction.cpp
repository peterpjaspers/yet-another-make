#include "Instruction.h"
#include "ThreadContext.h"
#include "Functions.h"
#include "Monitor.h"
#include "SymbolTable.h"
#include "Translator.h"
#include "Intrinsics.h"
#include "Translator.h"

#include <iostream>
#include <map>

// ToDo: Include symbol name when outputing monitoring info
// ToDo: Account for machine endianship

using namespace std;

namespace Language {

    void storeWordOperand( ThreadContext& ctx, const Address pc, const Word w ) {
        if (4 <= pageRemainder( pc )) {
            auto address( addressProgram( ctx, pc ) );
            *reinterpret_cast<Word*>( address ) = w;
        } else {
           *addressProgram( ctx, pc + 0 ) = ((w >>  0) & 0xFF);
           *addressProgram( ctx, pc + 1 ) = ((w >>  8) & 0xFF);
           *addressProgram( ctx, pc + 2 ) = ((w >> 16) & 0xFF);
           *addressProgram( ctx, pc + 3 ) = ((w >> 24) & 0xFF);
        }
    }
    Word loadWordOperand( const ThreadContext& cctx, const Address pc ) {
        if (4 <= pageRemainder( pc )) {
            auto address( addressProgram( cctx, pc ) );
            return( *reinterpret_cast<Word*>(address) );
        } else {
            Word byte0( *addressProgram( cctx, pc + 0 ) );
            Word byte1( *addressProgram( cctx, pc + 1 ) );
            Word byte2( *addressProgram( cctx, pc + 2 ) );
            Word byte3( *addressProgram( cctx, pc + 3 ) );
            return( byte3 << 24 || byte2 << 16 || byte1 << 8 || byte0 );
        }
    }
    inline Word fetchWordOperand( ThreadContext& ctx ) {
        auto operand( loadWordOperand( ctx, ctx.pc ) );
        ctx.pc += 4;
        return operand;
    }
    void storeDescriptorOperand( ThreadContext& ctx, const Address pc, const Descriptor d ) {
        if (8 <= pageRemainder( pc )) {
            auto address( addressProgram( ctx, pc ) );
            *reinterpret_cast<Descriptor*>( address ) = d;
        } else {
           *addressProgram( ctx, pc + 0 ) = ((d >>  0) & 0xFF);
           *addressProgram( ctx, pc + 1 ) = ((d >>  8) & 0xFF);
           *addressProgram( ctx, pc + 2 ) = ((d >> 16) & 0xFF);
           *addressProgram( ctx, pc + 3 ) = ((d >> 24) & 0xFF);
           *addressProgram( ctx, pc + 4 ) = ((d >> 32) & 0xFF);
           *addressProgram( ctx, pc + 5 ) = ((d >> 40) & 0xFF);
           *addressProgram( ctx, pc + 6 ) = ((d >> 48) & 0xFF);
           *addressProgram( ctx, pc + 7 ) = ((d >> 56) & 0xFF);
        }
    }
    Descriptor loadDescriptorOperand( const ThreadContext& cctx, const Address pc ) {
        if (8 <= pageRemainder( pc )) {
            auto address( addressProgram( cctx, pc ) );
            return( *reinterpret_cast<Descriptor*>(address) );
        } else {
            Descriptor byte0( *addressProgram( cctx, pc + 0 ) );
            Descriptor byte1( *addressProgram( cctx, pc + 1 ) );
            Descriptor byte2( *addressProgram( cctx, pc + 2 ) );
            Descriptor byte3( *addressProgram( cctx, pc + 3 ) );
            Descriptor byte4( *addressProgram( cctx, pc + 4 ) );
            Descriptor byte5( *addressProgram( cctx, pc + 5 ) );
            Descriptor byte6( *addressProgram( cctx, pc + 6 ) );
            Descriptor byte7( *addressProgram( cctx, pc + 7 ) );
            return( byte7 << 56 | byte6 << 48 | byte5 << 40 | byte4 << 32 | byte3 << 24 | byte2 << 16 | byte1 << 8 | byte0 );
        }
    }
    inline Descriptor fetchDescriptorOperand( ThreadContext& ctx ) {
        auto operand( loadDescriptorOperand( ctx, ctx.pc ) );
        ctx.pc += 8;
        return operand;
    }
    ostream& monitorInstruction( const ThreadContext& cctx, OpCode code ) {
        ostream& record = monitorRecord() << setw( 4 ) << cctx.pc << " - " << opCodeTable[ code ].name;
        if (opCodeTable[ code ].operand == 1) {
            auto operand( loadWordOperand( cctx, cctx.pc + sizeof( OpCode ) ) );
            if (code == OpPushReal) record << "( " << bit_cast<float>( operand ) << " )";
            else record << "( " << operand << " )";
        } else if (opCodeTable[ code ].operand == 2) {
            record << "( " << toReadable( loadDescriptorOperand( cctx, cctx.pc + sizeof( OpCode ) ) ) << " )";
        }
        return record;
    }
    void push( const Descriptor& descriptor ) { push( context(), descriptor ); }
    void push( ThreadContext& ctx, const Descriptor& descriptor ) {
        auto address( addressStack( ctx ) );
        if (monitor( DebugAspects::StackOperations )) monitorRecord() << "push(" << toReadable( descriptor ) << " )" << record<char>;
        *address = descriptor;
        ctx.sp += sizeof( Descriptor );
    }
    Descriptor pop() { return pop( context() ); }
    Descriptor pop( ThreadContext& ctx ) {
        auto d( *addressStack( ctx, sizeof( Descriptor ) ) );
        if (monitor( DebugAspects::StackOperations )) monitorRecord() << "pop()" << record<char>;
        #ifdef _DEBUG_INTERPRETER
            static const char* signature( "Descriptor pop()" );
            if (ctx.sp < sizeof( Descriptor )) throw string( signature ) + " - Stack underflow";
        #endif
        ctx.sp -= sizeof( Descriptor );
        return d;
    }
    // Return number of arguments passsed to current procedure.
    inline int argc( const ThreadContext& cctx ) {
        return( (cctx.fp - cctx.ap) / sizeof( Descriptor ) - 2 );
    }
    // Return array of arguments of current procedure.
    inline Descriptor* argv( const ThreadContext& cctx ) {
        return reinterpret_cast<Descriptor*>( addressMemory( cctx.stack, cctx.ap ) );
    }
    inline void jump( ThreadContext& ctx ) {
        Address transfer( fetchWordOperand( ctx ) );
        ctx.pc = transfer;
    }
    inline void conditionalJump( ThreadContext& ctx ) {
        Address transfer( fetchWordOperand( ctx ) );
        if (isNull( pop( ctx ) )) ctx.pc = transfer;
    }
    inline void exitProgram( ThreadContext& ctx ) {
        auto d( pop( ctx ) );
        toInteger( d );
        exit( integer( d ) );
    }
    // Mark start of argument expression evaluation
    inline void arguments( ThreadContext& ctx ) {
        push( ctx, AddressDescriptor( ctx.ep ) );
        ctx.ep = ctx.sp;
    }
    void procedureReturn( ThreadContext& ctx );
    inline void procedureCall( ThreadContext& ctx ) {
        static const char* signature( "void procedureCall()" );
        // Pick-up (procedure) descriptor, the procedure descriptor and saved expression pointer descriptor
        // are located under the current expression pointer (ep).
        Descriptor& transfer( *reinterpret_cast<Descriptor*>( addressMemory( ctx.stack, ctx.ep - (2 * sizeof( Descriptor )) ) ) );
        // ToDo: Procedure descriptor may be a file or a string in which case it must translated.
        bool intrin( false );
        if (isIntrinsic( transfer )) {
            if (monitor( DebugAspects::ProcedureCalls )) monitorRecord() << "Intrinsic call " << transfer << record<char>;
            intrin = true;
        } else {
            if (!isProcedure( transfer )) throw string( signature ) + " - Procedures call address invalid";
            if (monitor( DebugAspects::ProcedureCalls )) monitorRecord() << "Procedure call " << transfer << record<char>;
        }
        push( ctx, AddressDescriptor( ctx.pc ) );
        push( ctx, AddressDescriptor( ctx.fp ) );
        ctx.pc = address( transfer );
        ctx.fp = ctx.sp;
        ctx.ap = ctx.ep;
        if (intrin) {
            // Call the intrinsic (C++) function and push its return value on the stack.
            // Intrinsic function can access its parameters via argc and argv arguments.
            push( ctx, intrinsic( transfer )( argc( ctx ), argv( ctx ) ) );
            procedureReturn( ctx );
        }
    }
    inline void procedureReturn( ThreadContext& ctx ) {
        static const char* signature( "void procedureReturn()" );
        // Replace procedure call descriptor with procedure result
        Descriptor& result( *reinterpret_cast<Descriptor*>( addressMemory( ctx.stack, (ctx.ap - (2 * sizeof( Descriptor )) ) ) ) );
        result = pop( ctx );
        #ifdef _DEBUG_INTERPRETER
            if (monitor( DebugAspects::ProcedureCalls )) monitorRecord() << "Procedure return " << record<char>;
            ctx.sp = ctx.fp;
            auto fp( pop( ctx ) );
            auto pc( pop( ctx ) );
            if (!isAddress( pc ) or !isAddress( fp )) throw string( signature ) + " - Corrupt stack";
            ctx.pc = address( pc );
            ctx.fp = address( fp );
            ctx.sp = ctx.ap;
            auto ap( pop( ctx ) );
            if (!isAddress( ap )) throw string( signature ) + " - Corrupt stack";
            ctx.ep = ( ctx.ap = address( ap ) );
        #elif
            ctx.sp = ctx.fp;
            ctx.fp( address( pop( ctx ) ) );
            ctx.pc( address( pop( ctx ) ) );
            ctx.sp = ctx.ap;
            ctx.ep = ( ctx.ap = address( ap ) );
        #endif
    }
    inline void locals( ThreadContext& ctx ) {
        Word locals( fetchWordOperand( ctx ) );
        ctx.sp += locals;
    }
    inline void enteringAnonymousScope( ThreadContext& ctx ) {
        Word scope( fetchWordOperand( ctx ) );
        enterAnonymousScope( ctx, scope );
    }
    inline void enteringNamedScope( ThreadContext& ctx ) {
        static const char* signature( "void enteringNamedScope()" );
        Descriptor scope( fetchDescriptorOperand( ctx ) );
        if (typeCode( scope ) != TypeString) throw string( signature ) + " - Logic error, expected scope name";
        enterNamedScope( ctx, stringToCString( scope ) );
    }
    inline void exittingScope( ThreadContext& ctx ) { exitScope( ctx ); }
    inline void loadArgument( ThreadContext& ctx ) {
        #ifdef _DEBUG_INTERPRETER
            static const char* signature( "void loadArgument( ThreadContext& context )" );
            Word offset( fetchWordOperand( ctx ) );
            auto range( ctx.fp - ctx.ap );
            if (range <= offset) throw string( signature ) + " - Argument index out of range";
            push( ctx, *addressArgument( offset ) );
        #else
            push( ctx, *addressArgument( fetchWordOperand() ) );
        #endif
    }
    inline void loadLocal( ThreadContext& ctx ) {
        push( ctx, *addressLocal( ctx, fetchWordOperand( ctx ) ) );
    }
    inline void loadGlobal( ThreadContext& ctx ) {
        push( ctx, *addressGlobal( ctx, fetchWordOperand( ctx) ) );
    }
    inline void storeArgument( ThreadContext& ctx ) {
        #ifdef _DEBUG_INTERPRETER
            static const char* signature( "void storeArgument( ThreadContext& context )" );
            Word offset( fetchWordOperand( ctx ) );
            auto range( ctx.fp - ctx.ap );
            if (range <= offset) throw string( signature ) + " - Argument index out of range";
            *addressArgument( ctx, offset ) = pop( ctx );
        #else
            *addressArgument( ctx, fetchWordOperand( ctx ) ) = pop( ctx );
        #endif
    }
    inline void storeLocal( ThreadContext& ctx ) {
        *addressLocal( ctx, fetchWordOperand( ctx ) ) = pop( ctx );
    }
    inline void storeGlobal( ThreadContext& ctx ) {
        *addressGlobal( ctx, fetchWordOperand( ctx ) ) = pop( ctx );
    }
    inline void assign( ThreadContext& ctx ) {
        static const char* signature( "void assign( ThreadContext& context )" );
        auto rhs( pop( ctx ) );
        auto lhs( addressStack( ctx, sizeof( Descriptor ) ) );
        if (isLocalVariable( *lhs )) {
            *addressLocal( ctx, address( *lhs ) ) = rhs;
        } else if (isArgumentVariable( *lhs )) {
            *addressArgument( ctx, address( *lhs ) ) = rhs;
        } else if (isGlobalVariable( *lhs )) {
            *addressGlobal( ctx, address( *lhs ) ) = rhs;
        } else {
            throw string( signature ) + " - Corrupt stack"; 
        }
        *lhs = rhs;
    }
    inline void duplicate( ThreadContext& ctx ) {
        auto value( addressStack( ctx, sizeof( Descriptor ) ) );
        *(value + 1) = *value;
        ctx.sp += sizeof( Descriptor );
    }
    void dereferenceVariable( ThreadContext& ctx ) {
        auto variable( reinterpret_cast<Descriptor*>( addressStack( ctx, sizeof( Descriptor ) ) ) );
        while (isVariable( *variable )) *variable = dereference( ctx, *variable );
    }
    // Evaluate string(s) representing code
    void evaluateExpression( ThreadContext& ctx ) {
        static const char* signature( "void evaluateExpression()" );
        auto usage( currentMemoryUsage( ctx ) );
        try {
            auto program( pop( ctx ) );
            if (isVariable( program )) program = dereference( ctx, program );
            auto programText( stringToCString( toString( program ) ) );
            auto start( translate( ctx, programText ) );
            auto resume( ctx.pc );
            ctx.pc = start;
            while (executeInstruction( ctx )) {};
            ctx.pc = resume;
        } catch (...) {
            // ToDo: Less severe error handling...
            throw string( signature ) + " - Fatal eval error";
        }
        recoverMemory( ctx, usage );
    }
    bool executeInstruction() { return executeInstruction( context() ); }
    bool executeInstruction( ThreadContext& ctx ) {
        static const char* signature( "void executeInstruction()" );
        auto address( addressProgram( ctx, ctx.pc ) );
        OpCode code( *address );
        if (monitor( DebugAspects::InstructionExecution )) monitorInstruction( ctx, code ) << record<char>;
        ctx.pc += 1;
        switch ( code ) {
            case OpPushNull : push( ctx, NullDescriptor() ); break;
            case OpPushInteger : push( ctx, IntegerDescriptor( fetchWordOperand( ctx ) ) ); break;
            case OpPushReal : push( ctx, RealDescriptor( bit_cast<float>(fetchWordOperand( ctx )) ) ); break;
            case OpPushDescriptor : push( ctx, fetchDescriptorOperand( ctx ) ); break;
            case OpPop : pop( ctx ); break;
            case OpAdd : push( ctx, FunAdd() ); break;
            case OpSub : push( ctx, FunSub() ); break;
            case OpMul : push( ctx, FunMul() ); break;
            case OpDiv : push( ctx, FunDiv() ); break;
            case OpEq : push( ctx, FunEq() ); break;
            case OpNeq : push( ctx, FunNeq() ); break;
            case OpLt : push( ctx, FunLt() ); break;
            case OpLteq : push( ctx, FunLteq() ); break;
            case OpGt : push( ctx, FunGt() ); break;
            case OpGteq : push( ctx, FunGteq() ); break;
            case OpCompare : push( ctx, FunCompare() ); break;
            case OpNot : push( ctx, FunNot() ); break;
            case OpAnd : push( ctx, FunAnd() ); break;
            case OpOr : push( ctx, FunOr() ); break;
            case OpJump : jump( ctx ); break;
            case OpConditionalJump : conditionalJump( ctx ); break;
            case OpArguments : arguments( ctx ); break;
            case OpProcedureCall : procedureCall( ctx ); break;
            case OpReturn : procedureReturn( ctx ); break;
            case OpLocals : locals( ctx ); break;
            case OpLoadArgument : loadArgument( ctx ); break;
            case OpLoadLocal : loadLocal( ctx ); break;
            case OpLoadGlobal : loadGlobal( ctx ); break;
            case OpStoreArgument : storeArgument( ctx ); break;
            case OpStoreLocal : storeLocal( ctx ); break;
            case OpStoreGlobal : storeGlobal( ctx ); break;
            case OpAssign : assign( ctx ); break;
            case OpDup : duplicate( ctx ); break;
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
            case OpEnterScope : enteringAnonymousScope( ctx ); break;
            case OpEnterNamedScope : enteringNamedScope( ctx ); break;
            case OpExitScope : exittingScope( ctx ); break;
            case OpDereference : dereferenceVariable( ctx ); break;
            case OpEvaluate : evaluateExpression( ctx ); break;
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
            while (executeInstruction( ctx )) {};
            // Pick-up return value left behind on the stack, if any
            if (sizeof(Descriptor) < ctx.sp) {
                auto value( pop() );
                auto t( typeCode( value ) );
                return word( toInteger( value ) );
            } else return( 0 );
        }
        catch (string message) { cerr << message << endl; }
        catch (...) { cerr << "Exception thrown..." << endl; }
        return( -1 );
    }
    void storeInstruction( ThreadContext& ctx, const OpCode code ) {
        if (monitor( DebugAspects::GeneratedCode )) {
            monitorRecord() << setw( 20 ) << "-> " << toReadable( code ) << record<char>;
        }
        auto address( addressProgram( ctx, allocateProgram( ctx, sizeof( OpCode ) ) ) );
        *address = code;
    }
    void storeInstruction( ThreadContext& ctx, const OpCode code, const Word operand ) {
        if (monitor( DebugAspects::GeneratedCode )) {
            string opString;
            if (code == OpPushReal) opString = to_string( bit_cast<float>( operand ) );
            else opString = to_string( operand );
            monitorRecord() << setw( 20 ) << "-> " << toReadable( code ) << "[ " << opString << " ]" << record<char>;
        }
        auto pc( allocateProgram( ctx, sizeof( OpCode ) + sizeof( Word ) ) );
        *addressProgram( ctx, pc ) = code;
        storeWordOperand( ctx, ( pc + sizeof( OpCode ) ), operand );
    }
    void storeInstruction( ThreadContext& ctx, const OpCode code, const Descriptor operand ) {
        if (monitor( DebugAspects::GeneratedCode )) {
            monitorRecord() << setw( 20 ) << "-> " << toReadable( code ) << "[ " << toReadable( operand ) << " ]"<< record<char>;
        }
        auto pc( allocateProgram( ctx, sizeof( OpCode ) + sizeof( Descriptor ) ) );
        *addressProgram( ctx, pc ) = code;
        storeDescriptorOperand( ctx, ( pc + sizeof( OpCode ) ), operand );
    }

    string toReadable( const OpCode code ) { return opCodeTable[ code ].name; }

    void printProgram( const std::filesystem::path file, const Address start, const Address end ) {
        LogFile stream( file, false, false );
        printProgram( stream, start, end );
    }
    void printProgram( LogFile& stream, const Address start, const Address end ) {
        auto cctx( ccontext() );
        Address pc( start );
        Address extent( end );
        if (extent <= pc) extent = allocateProgram( 0 );
        while (pc < extent) {
            auto address( addressProgram( pc ) );
            OpCode code( *address );
            ostream& instruction = stream() << setw( 4 ) << pc << " : " << toReadable( code );
            pc += 1;
            if (opCodeTable[ code ].operand == 1) {
                auto operand( loadWordOperand( cctx, pc ) );
                if (code == OpPushReal) instruction << "( " << bit_cast<float>( operand ) << " )";
                else instruction << "( " << operand << " )";
                pc += 4;
            } else if (opCodeTable[ code ].operand == 2) {
                instruction << "( " << toReadable( loadDescriptorOperand( cctx, pc ) ) << " )";
                pc += 8;
            }
            instruction << record<char>;
        }
    }

} // namespace Language
