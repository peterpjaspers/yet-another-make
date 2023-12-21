#ifndef ACCESS_MONITOR_LOG_H
#define ACCESS_MONITOR_LOG_H

#include <string>
#include <sstream>
#include <codecvt>

namespace AccessMonitor {

    enum LogLevel {
        None = 0,
        Terse = 1,
        Normal = 2,
        Verbose = 3
    };

    LogLevel enableLog( std::string file, LogLevel level = LogLevel::Normal );
    LogLevel enableLog( LogLevel level = LogLevel::Normal );
    void disableLog();
    bool logging( LogLevel level = LogLevel::Normal );
    std::wostringstream& log();

    std::wostream& endLine( std::wostream& stream );

    std::wstring widen( const std::string& src );

} // namespace AccessMonitor

#endif // ACCESS_MONITOR_LOG_H
