#include <windows.h>
#include <string>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

using namespace std;
using namespace std::filesystem;

bool multithreaded = false;
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
    remove_all( directoryPath );
}

void doFileAccess( bool multithreaded, const string& directory ) {
    if (multithreaded) {
        auto t0 = jthread( worker, path( "." ) / directory / "fileAccessTest0" );
        auto t1 = jthread( worker, path( "." ) / directory / "fileAccessTest1" );
        auto t2 = jthread( worker, path( "." ) / directory / "fileAccessTest2" );
        auto t3 = jthread( worker, path( "." ) / directory / "fileAccessTest3" );
    } else {
        worker( path( "." ) / directory / "fileAccessTest" );
    }
}

bool condition( const string& argument ) {
    if ((argument == "t") || (argument == "T")) return true;
    if ((argument == "true") || (argument == "TRUE")) return true;
    return false;
}

int main( int argc, char* argv[] ) {
    if (2 < argc) directory = argv[ 2 ];
    if (1 < argc) multithreaded = condition( argv[ 1 ] );
    // cout << "Remote process running " << ((multithreaded) ? "multi-" : "single-" ) << "threaded on directory " << directory << endl;
    doFileAccess( multithreaded, directory );
};

