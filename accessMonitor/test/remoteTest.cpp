#include <windows.h>
#include <string>
#include <filesystem>
#include <fstream>
#include <thread>

using namespace std;
using namespace std::filesystem;

path directory( temp_directory_path() );

void fileAccess( const path& dataDirectory ) {
    create_directories( dataDirectory );
    ifstream nefile( dataDirectory / "nonExisting.txt" );
    nefile.close();
    ifstream ifile( dataDirectory / "moreJunk.txt" );
    ifile.close();
    ofstream file( dataDirectory / "junk.txt" );
    file << "Hello world!\n";
    file.close();
    this_thread::sleep_for(chrono::milliseconds(rand() % 17));;
    ofstream anotherFile( dataDirectory / "moreJunk.txt" );
    anotherFile << "Hello again!\n";
    anotherFile.close();
    CopyFileW( (dataDirectory / "moreJunk.txt").c_str(), (dataDirectory / "evenMoreJunk.txt").c_str(), false );
    remove( dataDirectory / "junk.txt" );
    rename( dataDirectory / "moreJunk.txt", dataDirectory / "yetMoreJunk.txt" );
}

void doFileAccess( int threads, const path& directory ) {
    if (1 < threads) {
        vector<thread> workers;
        for (int i = 0; i < threads; ++i) {
            wstringstream subdir;
            subdir << L"fileAccessTest" << i;
            workers.push_back( thread( fileAccess, path( "." ) / directory / subdir.str() ) );
        }
        for (int i = 0; i < threads; ++i) workers[ i ].join();
    } else {
        fileAccess( path( "." ) / directory / L"fileAccessTest" );
    }
}

wstring uniqueName( const wstring& name, unsigned long code ) {
    wstringstream unique;
    unique << name << L"_" << code;
    return unique.str();
}

int main( int argc, char* argv[] ) {
    int session = 1;
    int threads = 1;
    if (3 < argc) directory = argv[ 3 ];
    if (2 < argc) threads = atoi( argv[ 2 ] );
    if (1 < argc) session = atoi( argv[ 1 ] );
    doFileAccess( threads, ( directory / uniqueName( L"RemoteSession", session ) ) );
    return( 0 );
};

