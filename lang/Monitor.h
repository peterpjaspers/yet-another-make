#ifndef INTERPRETER_MONITOR_H
#define INTERPRETER_MONITOR_H

#include "LogFile.h"
#include "Types.h"

#include <filesystem>

namespace Language {

    static const unsigned long MaxFileName = 1024;
    
    void startMonitor( const std::filesystem::path& dir, const std::string& file, bool logTimes = true, bool logIntervals = false );
    LogFile<char>& monitor();
#ifdef _DEBUG_INTERPRETER
    bool monitor( const LogAspects aspects );
#else
    // Supress debug logging unconditionally.
    // All debug logging code will be completely removed by the optimizing compiler.
    inline bool monitor( const LogAspects aspects ) { return false; }
#endif
    LogRecord<char>& monitorRecord();
    void stopMonitor();

    // ToDo: Determine if ProcedureCall is useful
    enum DebugAspects {
        General                 = (1 << 1),
        InstructionExecution    = (1 << 2),     // Log instructions as they are executed
        StackOperations         = (1 << 3),     // Log all push/pop operations on the stack
        ProcedureCall           = (1 << 4),     // Log procedure calls
        GlobalAccess            = (1 << 5),     // Global memory access (heap address)
        ArgumentAccess          = (1 << 6),     // Argument memory access (argument-list offset access)
        LocalAccess             = (1 << 7),     // Local memory access (frame offset access)
        StackAccess             = (1 << 8),     // Stack memory access (expresssion values)
        ParserFunctions         = (1 << 9),     // Recursive parse routines
        GeneratedCode           = (1 << 10),     // Code generated during parsing
        SourceCode              = (1 << 11),    // Source code read during parsing
        Reserved                = (1 << 12),
    };

} // namespace AccessMonitor

#endif // INTERPRETER_MONITOR_H
