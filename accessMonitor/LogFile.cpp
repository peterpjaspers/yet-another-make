#include "LogFile.h"

#include <fstream>
#include <iostream>
#include <clocale>
#include <codecvt>
#include <cstdlib>
#include <windows.h>

using namespace std;
using namespace std::filesystem;

// ToDo: Conditionally compile logging code while compiling in debug controlled via NDEBUG
// ToDo: Free (delete) LogRecord for each thread.

namespace AccessMonitor {

    namespace {

        path uniqueLogFileName( const wstring& name, unsigned long code ) {
            wstringstream unique;
            unique << name << L"_" << code << L".log";
            return temp_directory_path() /unique.str();
        }
        path uniqueLogFileName( const wstring& name, unsigned long code1, unsigned long code2 ) {
            wstringstream unique;
            unique << name << L"_" << code1 << L"_" << code2 << L".log";
            return temp_directory_path() /unique.str();
        }

    }
    
    LogFile::LogFile( const path& file, bool time, bool interval ) : 
        enabledAspects( 0 ), logTime( time ), logInterval( interval ), previousTime( chrono::system_clock::now() )
    {
        const char* signature = "LogFile( const path& file, bool time, bool interval )";
        logFile = new wofstream( file );
        tlsRecordIndex = TlsAlloc();
        if (tlsRecordIndex == TLS_OUT_OF_INDEXES) throw string( signature ) + " - Could not allocate thread local storage!";
        logMutex = new mutex;
    }

    LogFile::LogFile( const std::filesystem::path& file, const unsigned long code, bool time, bool interval ) :
        LogFile( uniqueLogFileName( file.c_str(), code ), time, interval )
    {}
    LogFile::LogFile( const std::filesystem::path& file, const unsigned long code1, const unsigned long code2, bool time, bool interval ) :
        LogFile( uniqueLogFileName( file.c_str(), code1, code2 ), time, interval )
    {}

    LogFile::~LogFile() { close(); }

    void LogFile::close() {
        if (logFile != nullptr) { logFile->close(); delete( logFile ); logFile = nullptr; }
        if (logMutex != nullptr) { delete( logMutex ); logMutex = nullptr; }
    }

    // Return logging stream on enabled log.
    LogRecord& LogFile::operator()() {
        static const char* signature = "LogRecord& LogFile::operator()()";
        LogRecord* record = static_cast<LogRecord*>( TlsGetValue( tlsRecordIndex ) );
        if (record == nullptr) {
            record = new LogRecord( *this );
            TlsSetValue( tlsRecordIndex, record );
        }
        if (logTime || logInterval) {
            auto time = chrono::system_clock::now();
            if (logTime) (*record) << time << " : ";
            if (logInterval) (*record) << "[ " << fixed << setprecision( 3 ) << setw( 6 ) <<
                chrono::duration_cast<chrono::microseconds >(time - previousTime).count() / 1000.0 << " ms ] ";
            previousTime = time;
        }
        return( *record );
    }

    void LogFile::record( const wstring& string ) {
        const lock_guard<mutex> lock( *logMutex ); 
        *logFile << string << flush;
    }

    // Complete log entry and write to log file.
    wostream& record( wostream& stream ) {
        LogRecord& logRecord = static_cast<LogRecord&>( stream );
        logRecord << "\n";
        logRecord.record( logRecord.str() );
        logRecord.str( L"" );
        return logRecord;
    }

    // Convert ANSI string to wide character string.
    wstring widen(const string& src) {
        std::size_t len = src.size();
        wstring dst(len, 0);
        mbstowcs(&dst[0], src.c_str(), len);
        return dst;
    }

    // Convert wide character string to ANSI character string
    string narrow( const wstring& string ){
        static wstring_convert< codecvt_utf8_utf16< wstring::value_type >, wstring::value_type > utf16conv;
        return utf16conv.to_bytes( string );
    }

    wstring lasErrorString( unsigned int errorCode ) {
        LPWSTR buffer = nullptr;
        size_t size =
            FormatMessageW(
                (FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS),
                NULL,
                errorCode,
                MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),
                (LPWSTR)&buffer,
                0,
                NULL
            );
        wstring message( buffer, size );
        LocalFree(buffer);
        if (message.back() == '\n') message.pop_back();
        return message;
    }

} // namespace AccessMonitor
