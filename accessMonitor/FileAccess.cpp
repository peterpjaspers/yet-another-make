#include "FileAccess.h"

using namespace std;
using namespace std::chrono;

namespace AccessMonitor {

    FileAccess::FileAccess() : mode( AccessNone ), modes( AccessNone ) {}
    FileAccess::FileAccess( const FileAccessMode& accessMode, const FileTime& time ) : mode( accessMode), modes( accessMode), lastWriteTime( time ) {}

    wstring modeToString( FileAccessMode mode ) {
        auto string = wstring( L"" );
        if ((mode & AccessRead) != 0) string += L"Read";
        if ((mode & AccessWrite) != 0) string += L"Write";
        if ((mode & AccessDelete) != 0) string += L"Delete";
        if (string == L"") string = L"None";
        return string;
    }

    FileAccessMode stringToMode( const wstring& modeString ) {
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


}
