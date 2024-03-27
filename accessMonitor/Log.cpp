#include "Log.h"
#include "Process.h"

#include <fstream>
#include <iostream>
#include <chrono>
#include <clocale>
#include <codecvt>
#include <cstdlib>

using namespace std;
using namespace std::filesystem;

// ToDo: Conditionally compile logging code while compiling in debug controlled via NDEBUG

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
    
    Log::Log( const path& file, bool time, bool interval ) : 
        logTime( time ), logInterval( interval ), previousTime( chrono::system_clock::now() )
    {
        logFile = new wofstream( file );
        // static uint8_t UTF16BOMCodes[ 2 ] = { 0xFE, 0xFF };
        // static wchar_t* UTF16BOM = reinterpret_cast<wchar_t*>( &UTF16BOMCodes[ 0 ] );
        // *logFile << UTF16BOM;
        logMutex = new mutex;
        logRecords = new map<ThreadID,LogRecord*>;

    }

    Log::Log( const std::filesystem::path& file, const unsigned long code, bool time, bool interval ) :
        Log( uniqueLogFileName( file.c_str(), code ), time, interval )
    {}
    Log::Log( const std::filesystem::path& file, const unsigned long code1, const unsigned long code2, bool time, bool interval ) :
        Log( uniqueLogFileName( file.c_str(), code1, code2 ), time, interval )
    {}

    Log::~Log() { close(); }

    void Log::close() {
        if (logFile != nullptr) { logFile->close(); delete( logFile ); logFile = nullptr; }
        if (logMutex != nullptr) { delete( logMutex ); logMutex = nullptr; }
        if (logRecords != nullptr) {
            for ( auto record : *logRecords ) { delete record.second; }
            delete logRecords; logRecords = nullptr;
        }
    }

    Log& Log::operator=( Log&& other ) {
        logFile = other.logFile;
        logMutex = other.logMutex;
        logRecords = other.logRecords;
        enabledAspects = other.enabledAspects;
        logTime = other.logTime;
        logInterval = other.logInterval;
        previousTime = other.previousTime;
        other.logFile = nullptr;
        other.logMutex = nullptr;
        other.logRecords = nullptr;
        return *this;
    }

    // Return logging stream on enabled log.
    LogRecord& Log::operator()() {
        static const char* signature = "LogRecord& Log::operator()() const";
        LogRecord* record = (*logRecords)[ CurrentThreadID() ];
        if (record == nullptr) {
            record = new LogRecord( *this );
            (*logRecords)[ CurrentThreadID() ] = record;
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

    void Log::record( const wstring& string ) {
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
        int len = src.size();
        wstring dst(len, 0);
        mbstowcs(&dst[0], src.c_str(), len);
        return dst;
    }

    // Convert wide character string to ANSI character string
    string narrow( const wstring& string ){
        static wstring_convert< codecvt_utf8_utf16< wstring::value_type >, wstring::value_type > utf16conv;
        return utf16conv.to_bytes( string );
    }

    wstring GetLastErrorString() {
        DWORD errorMessageID = GetLastError();
        if(errorMessageID == 0) return L"";
        LPWSTR messageBuffer = nullptr;
        size_t size =
            FormatMessageW(
                (FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS),
                NULL,
                errorMessageID,
                MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),
                (LPWSTR)&messageBuffer,
                0,
                NULL
            );
        wstring message( messageBuffer, size );
        LocalFree(messageBuffer);
        if (message.back() == '\n') message.pop_back();
        return message;
    }


} // namespace AccessMonitor
