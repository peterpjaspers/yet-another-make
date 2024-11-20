#include "FileAccess.h"

using namespace std;
using namespace std::chrono;

namespace AccessMonitor {

    wstring fileAccessModeToString( FileAccessMode mode ) {
        auto string = wstring( L"" );
        if ((mode & AccessRead) != 0) string += L"Read";
        if ((mode & AccessWrite) != 0) string += L"Write";
        if ((mode & AccessDelete) != 0) string += L"Delete";
        if (string == L"") string = L"None";
        return string;
    }

    FileAccessMode stringToFileAccessMode( const wstring& modeString ) {
        FileAccessMode mode( AccessNone );
        wstring string( modeString );
        while (0 < string.size()) {
            if (string.starts_with( L"None" )) { string.erase( 0, 4 ); continue; }
            if (string.starts_with( L"Read" )) { mode |= AccessRead; string.erase( 0, 4 ); continue; }
            if (string.starts_with( L"Write" )) { mode |= AccessWrite; string.erase( 0, 5 ); continue;  }
            if (string.starts_with( L"Delete" )) { mode |= AccessDelete; string.erase( 0, 6 ); continue;  }
            break; // Unknown FileAccessMode seen, ignore remainder of string...
        }
        return( mode );
    }

    FileAccess::FileAccess() {
        state.mode = AccessNone;
        state.modes = AccessNone;
        state.success = 1;
        state.failures = 0;
    }
    FileAccess::FileAccess( const FileAccessMode accessMode, const FileTime time, const bool success ) {
        state.mode = accessMode;
        state.modes = accessMode;
        state.success = ((success) ? 1 : 0 );
        state.failures = ((success) ? 0 : 1 );
        lastWriteTime = time;
    }
    void FileAccess::mode( const FileAccessMode mode, const FileTime time, const bool success ) {
        if (success) {
            if ((mode & AccessDelete) != 0) state.mode = AccessDelete;
            else if ((mode & AccessWrite) != 0) state.mode = AccessWrite;
            else if (((mode & AccessRead) != 0) && ((state.mode & AccessDelete) == 0) && ((state.mode & AccessWrite) == 0)) state.mode = AccessRead;
            state.modes |= mode;
            state.success = 1;
        } else {
            state.mode = mode;
            state.success = 0;
            state.failures = 1;
        }
        if ((lastWriteTime < time) && ((mode & AccessRead) == 0)) lastWriteTime = time;
    }

}
