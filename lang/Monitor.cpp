#include "Monitor.h"

#include <map>
#include <mutex>

using namespace std;
using namespace filesystem;

namespace Language {

    namespace { LogFile<char>* monitorLog( nullptr ); }

#ifdef _DEBUG_INTERPRETER
    void startMonitor( const path& dir, const string& file, bool monitorLogTimes, bool monitorLogIntervals ) {
        if (monitorLog != nullptr) delete monitorLog;
        monitorLog = new LogFile<char>( dir / (file + ".log"), monitorLogTimes, monitorLogIntervals );
    }
    LogFile<char>& monitor() {
        static const char* signature = "LogFile<char>& monitor()";
        if (monitorLog == nullptr) throw runtime_error( string( signature ) + " - No interpreter debug monitorLog defined" );
        return( *monitorLog );
    }
    bool monitor( const LogAspects aspects ) {
        if (monitorLog == nullptr) return false;
        return (*monitorLog)( aspects );
    }
    LogRecord<char>& monitorRecord() { return monitor()(); }
    void stopMonitor() { if (monitorLog != nullptr) { delete monitorLog; monitorLog = nullptr; } }
#else
    LogFile<T>* startMonitor( const path& dir, unsigned long code, bool monitorLogTimes, bool monitorLogIntervals ) { return nullptr; }
    LogFile<T>& monitor() {
        static const char* signature = "LogFile<T>& monitor()";
        throw runtime_error( string( signature ) + " - Debug monitorLog deisabled for optimized session" );
    }
    bool monitor( const LogAspects aspects ) { return false; }
    LogRecord<char>& monitorRecord() {
        static const char* signature = "LogRecord<char>& monitorRecord()";
        throw runtime_error( string( signature ) + " - Debug monitorLog deisabled for optimized session" );
    }
    void stopMonitor() []
#endif

} // namespace AccessMonitor
