#ifndef ACCESS_MONITOR_LOG_FILE_H
#define ACCESS_MONITOR_LOG_FILE_H

#include <filesystem>
#include <chrono>
#include <string>
#include <fstream>
#include <sstream>
#include <mutex>

namespace AccessMonitor {

    // Multi-thread safe logger.
    //
    // LogFile files contain records teminated with a newline.
    // Each record can be composed as an output stream.
    //
    // For example:
    //
    //    log = LogFile( "logfile.log" );
    //    ...
    //    log() << "This is a log record" << record;
    //    log() << "The current time is " << std::chrono::system_clock::now << record;
    //    ...
    //
    // LogFile record streams are terminated with the 'record' IO manipulator.
    // Each thread has its own record stream in which to composed log records at it own pace.
    // The 'record' IO manipulator ensures thread-safe access to the log file.
    //
    // Each record may be tagged with a time and/or time interval since the previous record.
    // 

    typedef uint64_t LogAspects;

    class LogRecord;

    class LogFile {
    public:
        LogFile() = delete;
        LogFile( const std::filesystem::path& file, bool logTimes = false, bool logIntervals = false );
        LogFile( const LogFile& other ) = delete;
        LogFile( LogFile&& other ) = delete;
        LogFile& operator=( const LogFile& other ) = delete;
        LogFile& operator=( LogFile&& other ) = delete;
        ~LogFile();
        // Return a (wide) string stream in which to compose a log record.
        LogRecord& operator()();
        // Enable logging one or more aspects.
        inline LogAspects enable( const LogAspects aspects );
        // Disable logging one or more aspects.
        inline LogAspects disable( const LogAspects aspects );
        // Test if logging is enabled for a particular aspect.
        // Returns true is logging is enabled, false otherwise.
        inline bool operator()( const LogAspects aspects ) const;
    private:
        std::wofstream logFile;
        std::mutex logMutex;
        unsigned int tlsRecordIndex;
        LogAspects enabledAspects;
        std::chrono::system_clock::time_point previousTime;
        bool logTime;
        bool logInterval;
        void record( const std::wstring& string );
        friend class LogRecord;
    };

    inline LogAspects LogFile::enable( const LogAspects aspects ) {
        auto previous = enabledAspects;
        enabledAspects |= aspects;
        return previous;
    }
    inline LogAspects LogFile::disable( const LogAspects aspects ) {
        auto previous = enabledAspects;
        enabledAspects &= ~aspects;
        return previous;
    }
    inline bool LogFile::operator()( const LogAspects aspects ) const {
        if ((enabledAspects & aspects) != 0) return true;
        return false;
    }

    class LogRecord : public std::wostringstream {
    public:
        LogRecord() = delete;
        LogRecord( LogFile& log ) : logFile( log ) {};
        LogRecord( const LogRecord& other ) = delete;
        LogRecord( LogRecord&& other ) = delete;
        LogRecord& operator=( const LogRecord& other ) = delete;
        LogRecord& operator=( LogRecord&& other ) = delete;
        inline void record( std::wstring string ) { logFile.record( string ); };
    private:
        LogFile& logFile;
        friend std::wostream& record( std::wostream& record );
    };

    // Terminate a log record and write it to the log file.
    std::wostream& record( std::wostream& stream );

    // Convert ANSI string to a wide string
    std::wstring widen( const std::string& src );
    // Convert wide string to ANSI string
    std::string narrow( const std::wstring& string );

    // Returns last Windows error as string.
    std::wstring lastErrorString( unsigned int errorCode );

} // namespace AccessMonitor

#endif // ACCESS_MONITOR_LOG_FILE_H
