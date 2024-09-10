#include <windows.h>
#include <string>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

// ToDo: Figure out why debug logging is not working in multithreaded remote process.

using namespace std;
using namespace std::filesystem;

string directory = "RemoteSession";

void worker( const path directoryPath ) {
    current_path( temp_directory_path() );
    create_directories( directoryPath );
    ofstream file( directoryPath / "junk.txt" );
    file << "Hello world!\n";
    file.close();
    this_thread::sleep_for(chrono::milliseconds(rand() % 17));;
    ofstream anotherFile( directoryPath / "moreJunk.txt" );
    anotherFile << "Hello again!\n";
    anotherFile.close();
    CopyFileW( (directoryPath / "moreJunk.txt").c_str(), (directoryPath / "evenMoreJunk.txt").c_str(), false );
    remove( directoryPath / "junk.txt" );
    rename( directoryPath / "moreJunk.txt", directoryPath / "yetMoreJunk.txt" );
    error_code ec;
    remove_all( directoryPath, ec );
}

void doFileAccess( int threads, const string& directory ) {
    if (1 < threads) {
        vector<thread> workerThreads;
        for (int i = 0; i < threads; ++i) {
            wstringstream subdir;
            subdir << L"fileAccessTest" << i;
            workerThreads.push_back( thread( worker, path( "." ) / directory / subdir.str() ) );
        }
        for (int i = 0; i < threads; ++i) workerThreads[ i ].join();
    } else {
        worker( path( "." ) / directory / L"fileAccessTest" );
    }
}

int main( int argc, char* argv[] ) {
    int threads = 1;
    if (2 < argc) directory = argv[ 2 ];
    if (1 < argc) threads = atoi( argv[ 1 ] );
    doFileAccess( threads, directory );
    return( 0 );
};

