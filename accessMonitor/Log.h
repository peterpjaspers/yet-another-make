#ifndef ACCESS_MONITOR_LOG_H
#define ACCESS_MONITOR_LOG_H

#include "Process.h"

#include <filesystem>
#include <string>
#include <sstream>
#include <map>
#include <mutex>

namespace AccessMonitor {

    typedef uint64_t LogAspects;

    class LogRecord;

    class Log {
    public:
        Log() : logFile( nullptr ), logMutex( nullptr ), logRecords( nullptr ) {};
        // Create a log file.
        Log( const std::filesystem::path& file, bool time = false, bool interval = false );
        Log( const Log& other ) = delete;
        Log( Log&& other ) = delete;
        Log& operator=( const Log& other ) = delete;
        Log& operator=( Log&& other );
        ~Log();
        // Return a (wide) string stream in which to compose a log record.
        LogRecord& operator()();
        // Enable logging one or more aspects.
        inline LogAspects enable( const LogAspects aspects );
        // Disable logging one or more aspects.
        inline LogAspects disable( const LogAspects aspects );
        // Test if logging is enabled for a particular aspect.
        // Returns true is logging is enabled, false otherwise.
        inline bool operator()( const LogAspects aspects ) const;
        // Close log file (and release all its resources)
        void close();
    private:
        std::wofstream* logFile;
        std::mutex* logMutex; 
        std::map<ThreadID,LogRecord*>* logRecords;
        LogAspects enabledAspects;
        std::chrono::system_clock::time_point previousTime;
        bool logTime;
        bool logInterval;
        friend class LogRecord;
        void record( const std::wstring& string );
    };

    LogAspects Log::enable( const LogAspects aspects ) {
        auto previous = enabledAspects;
        enabledAspects |= aspects;
        return previous;
    }
    LogAspects Log::disable( const LogAspects aspects ) {
        auto previous = enabledAspects;
        enabledAspects &= ~aspects;
        return previous;
    }
    inline bool Log::operator()( const LogAspects aspects ) const {
        if (logFile != nullptr && ((enabledAspects & aspects) != 0)) return true;
        return false;
    }

    class LogRecord : public std::wostringstream {
    public:
        LogRecord( Log& logFile ) : log( logFile ) {};
        inline void record( std::wstring string ) { log.record( string ); };
    private:
        Log& log;
        friend std::wostream& record( std::wostream& record );
    };

    // Terminate a log record and write it to the log file.
    std::wostream& record( std::wostream& stream );

    // Convert ANSI string to a wide string
    std::wstring widen( const std::string& src );
    // Convert wide string to ANSI string
    std::string narrow( const std::wstring& string );

} // namespace AccessMonitor

#endif // ACCESS_MONITOR_LOG_H
