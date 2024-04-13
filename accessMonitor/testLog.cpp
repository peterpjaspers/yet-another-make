#include "LogFile.h"

#include <filesystem>

using namespace std;
using namespace std::filesystem;
using namespace AccessMonitor;

int main( int argc, char* argv[] ) {
    auto log = LogFile( temp_directory_path() / L"LogTest.log", true, true );
};

