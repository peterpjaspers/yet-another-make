#ifndef ACCESS_MONITOR_FILE_ACCESS_H
#define ACCESS_MONITOR_FILE_ACCESS_H

#include <chrono>
#include <string>

namespace AccessMonitor {

    typedef uint16_t FileAccessMode;
    typedef std::chrono::time_point<std::chrono::file_clock> FileTime;

    static const FileAccessMode AccessNone = (1 << 0);
    static const FileAccessMode AccessRead = (1 << 1);
    static const FileAccessMode AccessWrite = (1 << 2);
    static const FileAccessMode AccessDelete = (1 << 3);

    class FileAccess {
        struct {
            uint64_t mode       : 8;    // Effective (collapsed) file access mode
            uint64_t modes      : 8;    // All access modes to file
            uint64_t success    : 1;    // Success or failure of effective file access
            uint64_t failures   : 1;    // One or more file accesses failed
        } state;
        FileTime        lastWriteTime;  // Last write time on file
    public:
        FileAccess();
        FileAccess( const FileAccessMode accessMode, const FileTime time, const bool success = true );
        inline FileAccessMode mode() { return state.mode; }
        inline FileAccessMode modes() { return state.modes; }
        inline bool success() { return state.success; }
        inline bool failures() { return state.failures; }
        inline FileTime writeTime() { return lastWriteTime; }
        void mode( const FileAccessMode mode, const FileTime time, const bool success = true );
    };

    // Convert file access mode to human readable string
    std::wstring fileAccessModeToString( FileAccessMode mode );
    // Convert human readable string to file access mode
    FileAccessMode stringToFileAccessMode( const std::wstring& modeString );

}

#endif ACCESS_MONITOR_FILE_ACCESS_H
