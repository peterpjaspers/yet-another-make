#include "../Translator.h"
#include "../Monitor.h"
#include "../Instruction.h"
#include "../SymbolTable.h"

#include <iostream>

using namespace std;
using namespace std::filesystem;

using namespace Language;

int main( int argc, char* argv[] ) {
    startMonitor( ".", "Parser", true, false );
    monitor().enable(
        DebugAspects::ParserFunctions |
        DebugAspects::GeneratedCode |
        DebugAspects::SourceCode |
        DebugAspects::Symbols
    );
    auto start( translate( path( argv[ 1 ] ) ) );
    stopMonitor();
    printProgram( "GeneratedCode.log" );
    printSymbolTable( "Symbols.log" );
    startMonitor( ".", "Interpreter", true, false );
    monitor().enable(
        DebugAspects::InstructionExecution |
        // DebugAspects::StackOperations |
        // DebugAspects::GlobalAccess |
        // DebugAspects::ArgumentAccess |
        // DebugAspects::LocalAccess |
        // DebugAspects::StackAccess |
        // DebugAspects::ProcedureCalls |
        DebugAspects::IntrinsicCalls |
        DebugAspects::Reserved
    );
    auto result( run( start ) );
    cout << "Program returned " << result << endl;
    stopMonitor();
}
