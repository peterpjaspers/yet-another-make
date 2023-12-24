#ifndef FILE_ACCESS_EVENTS_H
#define FILE_ACCESS_EVENTS_H

#include <string>
#include <chrono>
#include <map>

namespace AccessMonitor {

    static const unsigned long MaxFileName = 1024;

    typedef unsigned long FileAccessMode;

    static const FileAccessMode AccessNone = (1 << 1);
    static const FileAccessMode AccessRead = (1 << 1);
    static const FileAccessMode AccessWrite = (1 << 2);
    static const FileAccessMode AccessDelete = (1 << 3);
    static const FileAccessMode AccessVariable = (1 << 4);
    static const FileAccessMode AccessStopMonitoring = (1 << 10);

    std::wstring modeString( FileAccessMode mode );

    typedef std::chrono::time_point<std::chrono::system_clock> FileTime;

    struct FileAccessState {
        FileAccessMode accessedModes;
        FileTime lastWriteTime;
        FileAccessState();
        FileAccessState( const FileAccessMode& m, const FileTime& t );
    };

    void recordFileEvent( const std::wstring& f, FileAccessMode m, const FileTime& t );
    const std::map< std::wstring, FileAccessState > collectFileEvents();
    void streamAccessedFiles( std::wostream& stream );

}

#endif // FILE_ACCESS_EVENTS_H
