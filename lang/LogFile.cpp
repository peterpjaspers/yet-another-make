#include "LogFile.h"

#include <clocale>
#include <codecvt>

using namespace std;
using namespace std::filesystem;

namespace Language {

    // Convert ANSI string to wide character string.
    wstring widen(const string& src) {
        auto len( src.size() );
        wstring dst(len, 0);
        mbstowcs(&dst[0], src.c_str(), len); // ToDo: use mbsrtowcs
        return dst;
    }

    // Convert wide character string to ANSI character string
    string narrow( const wstring& string ){
        static wstring_convert< codecvt_utf8_utf16< wstring::value_type >, wstring::value_type > utf16conv;
        return utf16conv.to_bytes( string );
    }

    string errorString( unsigned int errorCode ) {
        // Convert Windows error code to message
        LPSTR buffer = nullptr;
        size_t size =
            FormatMessageA(
                (FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS),
                NULL,
                errorCode,
                MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),
                (LPSTR)&buffer,
                0,
                NULL
            );
        string message( buffer, size );
        LocalFree(buffer);
        if (message.back() == '\n') message.pop_back();
        return message;
    }
    wstring werrorString( unsigned int errorCode ) {
        // Convert Windows error code to message
        LPWSTR buffer = nullptr;
        size_t size =
            FormatMessageW(
                (FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS),
                NULL,
                errorCode,
                MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),
                (LPWSTR)&buffer,
                0,
                NULL
            );
        wstring message( buffer, size );
        LocalFree(buffer);
        if (message.back() == '\n') message.pop_back();
        return message;
    }

} // namespace AccessMonitor
