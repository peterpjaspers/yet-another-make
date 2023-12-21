#ifndef MONITOR_FILES_H
#define MONITOR_FILES_H

#include <filesystem>
#include <ostream>

namespace AccessMonitor {

    static const unsigned long MaxFileName = 1024;

    typedef unsigned long FileAccessMode;

    static const FileAccessMode AccessNone = (1 << 1);
    static const FileAccessMode AccessRead = (1 << 1);
    static const FileAccessMode AccessWrite = (1 << 2);
    static const FileAccessMode AccessDelete = (1 << 3);
    static const FileAccessMode AccessVariable = (1 << 4);

    typedef std::chrono::time_point<std::chrono::system_clock> FileTime;

    struct FileAccessState {
        FileTime lastWriteTime;
        FileAccessMode accessedModes;
        FileAccessState();
    };

    // Register patches for OS file access functions
    void registerFileAccess();
    // Unregister patches for OS file access functions
    void unregisterFileAccess();

    void streamAccessedFiles( std::wostream& stream );

}

#endif // MONITOR_FILES_H
