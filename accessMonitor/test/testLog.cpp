#include "../LogFile.h"

#include <filesystem>

using namespace std;
using namespace std::filesystem;
using namespace AccessMonitor;

int main( int argc, char* argv[] ) {
    path temp( temp_directory_path() );
    auto log = LogFile( temp / L"LogTest.log", true, true );
};

