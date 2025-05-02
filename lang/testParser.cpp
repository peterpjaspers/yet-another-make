#include "Translator.h"

#include "Monitor.h"
#include "Instruction.h"
#include "SymbolTable.h"

using namespace std;
using namespace std::filesystem;

using namespace Language;

// ToDo: Move test programs to test subdirectory

int main( int argc, char* argv[] ) {
    startMonitor( ".", "Parser", true, false );
    monitor().enable(
        DebugAspects::ParserFunctions |
        DebugAspects::GeneratedCode |
        DebugAspects::SourceCode
    );
    auto start( translate( path( argv[ 1 ] ) ) );
    stopMonitor();
    printProgram( "GeneratedCode.txt" );
    printSymbolTable( "Symbols.txt" );
    startMonitor( ".", "Interpreter", true, false );
    monitor().enable(
        DebugAspects::InstructionExecution |
        // DebugAspects::StackOperations |
        // DebugAspects::GlobalAccess |
        // DebugAspects::ArgumentAccess |
        // DebugAspects::LocalAccess |
        // DebugAspects::StackAccess |
        // DebugAspects::ProcedureCall |
        DebugAspects::Reserved
    );
    auto result( run( start ) );
    cout << "Program returned " << result << endl;
    stopMonitor();
}
