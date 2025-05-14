#include "Instruction.h"

#include "Monitor.h"

using namespace Language;

int main( int argc, char* argv[] ) {
    startMonitor( ".", "Language", true, false );
    monitor().enable(
        DebugAspects::InstructionExecution |
        DebugAspects::StackOperations |
        DebugAspects::ProcedureCall |
        DebugAspects::GlobalAccess |
        DebugAspects::StackAccess
    );
    ThreadContext context;
    storeInstruction( OpArguments );                    // 0
    storeInstruction( OpPushInteger, Word(3) );         // 1
    storeInstruction( OpPushInteger, Word(5) );         // 6
    storeInstruction( OpProcedureCall, Address(17) );   // 11
    storeInstruction( OpExit );                         // 16
    storeInstruction( OpLocals, Word(0) );              // 17
    storeInstruction( OpLoadArgument, Word(0) );        // 22
    storeInstruction( OpLoadArgument, Word(8) );        // 27
    storeInstruction( OpLt );                           // 32
    storeInstruction( OpConditionalJump, Address(48) ); // 33
    storeInstruction( OpPushInteger, Word(3) );         // 38
    storeInstruction( OpJump, Address(53) );            // 43
    storeInstruction( OpPushInteger, Word(5) );         // 48
    storeInstruction( OpReturn );                       // 53
    printProgram( "Program.txt" );
    while (true) executeInstruction( context );
}
