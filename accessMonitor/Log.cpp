#include "Log.h"
#include "Process.h"

#include <fstream>
#include <chrono>
#include <clocale>
#include <cstdlib>

using namespace std;

// ToDo: Fix bug (presumably due to multi=threaded logging)
// terminate called after throwing an instance of 'std::logic_error'
//  what():  basic_string::_M_construct null not valid

// ToDo: Conditionally compile logging code while compiling in debug controlled via NDEBUG

namespace AccessMonitor {
    
    // ToDo: Implement multi-thread safe access to log file, thread_local does not work
    namespace {
        static wostream* logFile = nullptr;
//        thread_local wostringstream* logStream = nullptr;
        static wostringstream* logStream = nullptr;
        static LogLevel logLevel = LogLevel::Normal;
        bool levelValid( LogLevel level ) { return ((level == Terse) || (level == Normal) || (level == Verbose)); }
    }

    // Enable logging to file.
    LogLevel enableLog( std::string file, LogLevel level ) {
        static const char* signature = "void enableLog( std::string file, LogLevel level )";
        if (logFile != nullptr) throw string( signature ) + " - Enabling enabled Monitor log on " + file;
        if (!levelValid( level )) throw string( signature ) + " - Invalid logging level";
        // Add process PID to file name to generate unique file name for log file...
        stringstream uniqueFile;
        uniqueFile << file << "_" << hex << CurrentProcessID() << ".log";
        logFile = new wofstream( uniqueFile.str() );
/*
        // ToDo: Write BOM to log file (this code does not work for some reason!)
        static uint8_t UTF16BOM[ 2 ] = { 0xFE, 0xFF }; 
        logStream->write( reinterpret_cast<wchar_t*>(&UTF16BOM[ 0 ]), 2 );
*/
        logLevel = level;
        return( logLevel );
    }

    // Switch logging level.
    // Switch to LogLevel::None to (temporarily suppress logging).
    LogLevel enableLog( LogLevel level ) {
        static const char* signature = "void enableLog( LogLevel level )";
        if (!levelValid( level )) throw string( signature ) + " - Invalid logging level!";
        LogLevel oldLevel = logLevel;
        logLevel = level;
        return( oldLevel );
    }

    // Disable logging and close log file.
    void disableLog() {
        static const char* signature = "void disableLog()";
        if (logFile == nullptr) throw string( signature ) + " - Disabling disabled Monitor log!";
        static_cast<wfstream*>(logFile)->close();
        delete logFile;
        logFile = nullptr;
    };

    // Test if logging is enabled for a particular level.
    // Returns true is logging is enabled for level or lower, false otherwise.
    bool logging( LogLevel level ) { return( (logFile != nullptr) && (level <= logLevel) ); }

    // Return logging stream on enabled log.
    wostringstream& log() {
        static const char* signature = "wostringstream& log()";
        if (logFile == nullptr) throw string( signature ) + " - Monitor log not enabled!";
        if (logStream == nullptr) logStream = new wostringstream();
        (*logStream) << chrono::system_clock::now() << " : ";
        return( *logStream );
    }

    // Complete log entry and write to log file.
    wostream& endLine( wostream& stream ) {
        wostringstream& sstream = static_cast<wostringstream&>( stream );
        sstream << "\n";
        *logFile << sstream.str() << flush;
        sstream.str( L"" );
        return stream;
    }

    // Convert ANSI string to wide character string.
    wstring widen(const string& src) {
        int len = src.size();
        wstring dst(len + 1, 0);
        mbstowcs(&dst[0], src.c_str(), len);
        return dst;
    }

} // namespace AccessMonitor
