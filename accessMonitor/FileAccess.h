#ifndef ACCESS_MONITOR_FILE_ACCESS_H
#define ACCESS_MONITOR_FILE_ACCESS_H

#include <chrono>
#include <string>

namespace AccessMonitor {

    typedef unsigned long FileAccessMode;
    typedef std::chrono::time_point<std::chrono::file_clock> FileTime;

    static const FileAccessMode AccessNone = (1 << 0);
    static const FileAccessMode AccessRead = (1 << 1);
    static const FileAccessMode AccessWrite = (1 << 2);
    static const FileAccessMode AccessDelete = (1 << 3);

    struct FileAccess {
        FileAccessMode  mode;
        FileTime        lastWriteTime;
        FileAccess();
        FileAccess( const FileAccessMode& mode, const FileTime& time );
    };

    // Convert file access mode to human readable string
    std::wstring modeToString( FileAccessMode mode );
    // Convert human readable string to file access mode
    FileAccessMode stringToMode( const std::wstring& modeString );

}

#endif ACCESS_MONITOR_FILE_ACCESS_H
