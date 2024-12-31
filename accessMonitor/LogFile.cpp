#include "LogFile.h"

#include <fstream>
#include <clocale>
#include <codecvt>
#include <cstdlib>
#include <windows.h>

using namespace std;
using namespace std::filesystem;

// ToDo: Free (delete) LogRecord for each thread.
//       Low priority because only a problen for threads that start and stop monioring
//       and therefore not a problem for threads in a thread pool.

namespace AccessMonitor {

    LogFile::LogFile( const path& file, bool time, bool interval ) : 
        logFile( file ), tlsRecordIndex( TlsAlloc() ), enabledAspects( 0 ), logTime( time ), logInterval( interval ), previousTime( chrono::system_clock::now() )
    {
        const char* signature = "LogFile( const path& file, bool time, bool interval )";
        if (tlsRecordIndex == TLS_OUT_OF_INDEXES) throw runtime_error( string( signature ) + " - Could not allocate thread local storage!" );
    }
    LogFile::~LogFile() {
        const lock_guard<mutex> lock( logMutex );
        logFile.close();
    }
    // Return logging stream on enabled log.
    LogRecord& LogFile::operator()() {
        static const char* signature = "LogRecord& LogFile::operator()()";
        auto record( static_cast<LogRecord*>( TlsGetValue( tlsRecordIndex ) ) );
        if (record == nullptr) {
            record = new LogRecord( *this );
            TlsSetValue( tlsRecordIndex, record );
        }
        if (logTime || logInterval) {
            auto time( chrono::system_clock::now() );
            if (logTime) (*record) << time << " : ";
            if (logInterval) (*record) << "[ " << fixed << setprecision( 3 ) << setw( 6 ) <<
                chrono::duration_cast<chrono::microseconds >(time - previousTime).count() / 1000.0 << " ms ] ";
            previousTime = time;
        }
        return( *record );
    }

    void LogFile::record( const wstring& string ) {
        const lock_guard<mutex> lock( logMutex ); 
        logFile << string << flush;
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

    wstring errorString( unsigned int errorCode ) {
        // Convert Windows error code to message
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
