#include <string>
#include <vector>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <thread>

#include <windows.h>
#include <psapi.h>
#include <imagehlp.h>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "imagehlp.lib")

namespace AccessMonitor {

    struct ModuleFile {
        std::string name;
        std::string directory;
        ModuleFile( std::string& n, std::string& d ) : name( n ), directory( d ) {}
    };

    // Convert Windows error code to string
    std::string errorString( DWORD error ) {
        if (0 < error) {
            void* message;
            DWORD flags = (FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS);
            DWORD length = FormatMessage( flags, nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &message, 0, nullptr );
            if (0 < length) {
                std::string errorString( (LPCSTR)message, ((LPCSTR)message + length) );
                LocalFree( message );
                return errorString;
            }
        }
        return std::string( "SUCCESS" );
    }

    // Generate exception with function name and  Windows last error code
    void errorException( const std::string function ) {
        DWORD error = GetLastError();
        if (0 < error) throw runtime_error( function + errorString( error ) );
    }
    
    
    // Enumerate all DLLs with a matching path
    std::vector<ModuleFile> EnumerateModules( std::string match ) {
        static const unsigned int MaxModules = 100;
        std::vector<ModuleFile> dlls;
        HANDLE p = GetCurrentProcess();
        HMODULE modules[ MaxModules ];
        DWORD dataSize;
        bool enumerated = EnumProcessModules( p, &modules[0], DWORD(100 * sizeof( HMODULE )), &dataSize );
        if (!enumerated) errorException( "EnumProcessModules" );
        int moduleCount = (dataSize / sizeof( HMODULE ));
        for (int m = 0; m < moduleCount; ++m) {
            char fullName[ MAX_PATH ];
            int length = GetModuleFileNameA( modules[ m ], fullName, MAX_PATH );
            if (length == 0) errorException( "GetModuleFileNameA" );
            std::string path( fullName );
            if ( (0 < match.size()) && (path.size() <= path.find( match )) ) continue;
            std::size_t pos = path.rfind( "\\" );
            std::string dir( path, 0, pos );
            std::string base( path, (pos + 1), path.size() );
            dlls.push_back( ModuleFile( base, dir ) );
        }
        return dlls;
    }
    std::vector<ModuleFile> EnumerateModules() { return EnumerateModules( "" ); }

    std::vector<std::string> EnumerateExports( ModuleFile& dll ) {
        std::vector<std::string> exports;
        _LOADED_IMAGE image;
        bool mapped = MapAndLoad( dll.name.c_str(), dll.directory.c_str(), &image, true, true );
        if (!mapped) errorException( "MapAndLoad" );
        unsigned long dataSize;
        IMAGE_SECTION_HEADER* sectionHeader;
        IMAGE_EXPORT_DIRECTORY* directory = (_IMAGE_EXPORT_DIRECTORY*)ImageDirectoryEntryToDataEx( image.MappedAddress, true, IMAGE_DIRECTORY_ENTRY_EXPORT, &dataSize, &sectionHeader );
        if (directory != nullptr) {
            DWORD* names = (DWORD*)ImageRvaToVa( image.FileHeader, image.MappedAddress, directory->AddressOfNames, nullptr );
            if  (names == nullptr) { UnMapAndLoad( &image ); errorException( "ImageRvaToVa" ); }
            for (int n = 0; n < directory->NumberOfNames; ++n) {
                char* name = (char*)ImageRvaToVa( image.FileHeader, image.MappedAddress, names[ n ], nullptr );
                if (name == nullptr) { UnMapAndLoad( &image ); errorException( "ImageRvaToVa" );
                } else exports.push_back( name );
            }
        }
        UnMapAndLoad( &image );
        return exports;
    }
}

using namespace std;
using namespace AccessMonitor;

int main( int argc, char* argv[] ) {
    ofstream out( ( (2 < argc) ? argv[ 2 ] : "externals.txt" ) );
    uint32_t index = 1;
    string library = ( (1 < argc) ? argv[ 1 ] : "" );
    if (library != "") {
        HANDLE lib = LoadLibraryA( library.c_str() );
        if (lib == nullptr) throw runtime_error( string( "Could not load library" ) + library.c_str() + "!" );
    }
    auto dlls = EnumerateModules( "" );
    for ( auto dll : dlls ){
        out << "Module " << dll.name << " in " << dll.directory << "\n";
        auto exports = EnumerateExports( dll );
        for ( auto exported : exports ) out << setw(7) << index++ << "   " << exported << "\n";
    }
    out.close();
    this_thread::sleep_for( 2000ms );
    return( 0 );
}
