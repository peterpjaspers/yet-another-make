#ifndef LANG_LOG_FILE_H
#define LANG_LOG_FILE_H

#include <filesystem>
#include <chrono>
#include <string>
#include <fstream>
#include <sstream>
#include <mutex>
#include <cstdlib>
#include <windows.h>

// ToDo: Remove mutex for implementations that do not require multi-threaded log-file access.

namespace Language {

    // Multi-thread safe logger.
    //
    // BasicLogFile files contain records teminated with a newline.
    // Each record can be composed as an output stream.
    //
    // For example:
    //
    //    log = BasicLogFile<char>( "logfile.log" );
    //    ...
    //    log() << "This is a log record" << record;
    //    log() << "The current time is " << std::chrono::system_clock::now << record;
    //    ...
    //
    // BasicLogFile record streams are terminated with the 'record' IO manipulator.
    // Each thread has its own record stream in which to composed log records at it own pace.
    // The 'record' IO manipulator ensures thread-safe access to the log file.
    //
    // Each record may be tagged with a time and/or time interval since the previous record.
    // 

    typedef uint64_t LogAspects;

    template< class T> class BasicLogRecord;

    template< class T>
    class BasicLogFile {
    public:
        BasicLogFile() = delete;
        BasicLogFile( const std::filesystem::path& file, bool logTimes = false, bool logIntervals = false );
        BasicLogFile( const BasicLogFile<T>& other ) = delete;
        BasicLogFile( BasicLogFile<T>&& other ) = delete;
        ~BasicLogFile();
        BasicLogFile<T>& operator=( const BasicLogFile<T>& other ) = delete;
        BasicLogFile<T>& operator=( BasicLogFile<T>&& other ) = delete;
        // Return a (wide) string stream in which to compose a log record.
        BasicLogRecord<T>& operator()();
        // Enable logging one or more aspects.
        inline LogAspects enable( const LogAspects aspects );
        // Disable logging one or more aspects.
        inline LogAspects disable( const LogAspects aspects );
        // Test if logging is enabled for a particular aspect.
        // Returns true is logging is enabled, false otherwise.
        inline bool operator()( const LogAspects aspects ) const;
        // Remove current thread from access to log file.
        void removeThread() const;
    private:
        std::basic_ofstream<T> logFile;
        std::mutex logMutex;
        unsigned int tlsRecordIndex;
        LogAspects enabledAspects;
        std::chrono::system_clock::time_point previousTime;
        bool logTime;
        bool logInterval;
        void record( const std::basic_string<T>& string );
        friend class BasicLogRecord<T>;
    };

    typedef BasicLogFile<char> LogFile;
    typedef BasicLogFile<wchar_t> WLogFile;

    template< class T>
    inline LogAspects BasicLogFile<T>::enable( const LogAspects aspects ) {
        auto previous = enabledAspects;
        enabledAspects |= aspects;
        return previous;
    }
    template< class T>
    inline LogAspects BasicLogFile<T>::disable( const LogAspects aspects ) {
        auto previous = enabledAspects;
        enabledAspects &= ~aspects;
        return previous;
    }
    template< class T>
    inline bool BasicLogFile<T>::operator()( const LogAspects aspects ) const {
        if ((enabledAspects & aspects) != 0) return true;
        return false;
    }

    template< class T >
    BasicLogFile<T>::BasicLogFile( const std::filesystem::path& file, bool time, bool interval ) : 
        logFile( file ), tlsRecordIndex( TlsAlloc() ), enabledAspects( 0 ), logTime( time ), logInterval( interval ), previousTime( std::chrono::system_clock::now() )
    {
        const char* signature("BasicLogFile( const path& file, bool time, bool interval )" );
        if (tlsRecordIndex == TLS_OUT_OF_INDEXES) throw std::runtime_error( std::string( signature ) + " - Could not allocate thread local storage!" );
    }
    template< class T >
    BasicLogFile<T>::~BasicLogFile() {
        const std::lock_guard<std::mutex> lock( logMutex );
        logFile.close();
        TlsFree( tlsRecordIndex );
    }
    // Return logging stream on enabled log.
    template< class T >
    BasicLogRecord<T>& BasicLogFile<T>::operator()() {
        static const char* signature( "BasicLogRecord& BasicLogFile::operator()()" );
        auto record( static_cast<BasicLogRecord<T>*>( TlsGetValue( tlsRecordIndex ) ) );
        if (record == nullptr) {
            record = new BasicLogRecord<T>( *this );
            TlsSetValue( tlsRecordIndex, record );
        }
        if (logTime || logInterval) {
            auto time( std::chrono::system_clock::now() );
            if (logTime) (*record) << time << " : ";
            if (logInterval) (*record) << "[ " << std::fixed << std::setprecision( 3 ) << std::setw( 6 ) <<
                std::chrono::duration_cast<std::chrono::microseconds>(time - previousTime).count() / 1000.0 << " ms ] ";
            previousTime = time;
        }
        return( *record );
    }
    template< class T >
    void BasicLogFile<T>::removeThread() const {
        auto record( static_cast<BasicLogRecord<T>*>( TlsGetValue( tlsRecordIndex ) ) );
        if (record != nullptr) free( record );
        TlsSetValue( tlsRecordIndex, nullptr );
    }

    template< class T >
    void BasicLogFile<T>::record( const std::basic_string<T>& string ) {
        const std::lock_guard<std::mutex> lock( logMutex ); 
        if (logFile.is_open()) logFile << string << std::flush;
    }

    template< class T >
    // Complete log entry and write to log file.
    std::basic_ostream<T>& record( std::basic_ostream<T>& stream ) {
        auto& entry( static_cast<BasicLogRecord<T>&>( stream ) );
        entry << "\n";
        entry.record( entry.str() );
        entry.str( "" );
        return entry;
    }

    template< class T>
    class BasicLogRecord : public std::basic_ostringstream<T> {
    public:
        BasicLogRecord() = delete;
        BasicLogRecord( BasicLogFile<T>& log ) : logFile( log ) {};
        BasicLogRecord( const BasicLogRecord<T>& other ) = delete;
        BasicLogRecord( BasicLogRecord<T>&& other ) = delete;
        BasicLogRecord<T>& operator=( const BasicLogRecord<T>& other ) = delete;
        BasicLogRecord<T>& operator=( BasicLogRecord<T>&& other ) = delete;
        inline void record( std::basic_string<T> string ) { logFile.record( string ); };
    private:
        BasicLogFile<T>& logFile;
    };

    typedef BasicLogRecord<char> LogRecord;
    typedef BasicLogRecord<wchar_t> WLogRecord;

    // Convert ANSI string to a wide string
    std::wstring widen( const std::string& src );
    // Convert wide string to ANSI string
    std::string narrow( const std::wstring& string );

    // Convert OS error code to human readable error message.
    std::string errorString( unsigned int errorCode );
    std::wstring werrorString( unsigned int errorCode );

} // namespace AccessMonitor

#endif // LANG_LOG_FILE_H
