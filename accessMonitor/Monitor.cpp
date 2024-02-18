#include "Monitor.h"

#include <windows.h>
#include <sstream>
#include <fstream>
#include <filesystem>

using namespace std;
using namespace std::filesystem;

namespace AccessMonitor {

    Log monitorLog( "MonitorLog", true, true );

    namespace {

        // Remove contents of data directory, creating directory if it does not exist.
        void emptyDataDirectory() {
            const path monitorDirectory( temp_directory_path() / L"AccessMonitorData" );
            create_directory( monitorDirectory );
            for ( auto entry : directory_iterator( monitorDirectory ) ) {
                auto file = entry.path();
                if ( is_regular_file( file ) ) remove( file );
            }
        }
    }

    void startMonitoring() {
        static const char* signature = "void AccessMonitor::startMonitoring()";
        emptyDataDirectory();
        // Inject DLL for actual monitoring...
        HMODULE module = LoadLibraryW( L"C:\\Users\\philv\\Code\\yam\\yet-another-make\\accessMonitor\\patchDLL.dll" );
        if (module == nullptr) throw string( signature ) + string( "DLL load failed with error code " ) + to_string( GetLastError() ) + string( "!" );
        auto monitor = AccessEvent( "Monitor", CurrentProcessID() );
        auto exit = AccessEvent( "ExitProcess", CurrentProcessID() );
        EventWait( monitor );
    }

    void stopMonitoring() {
        auto monitor = AccessEvent( "Monitor", CurrentProcessID() );
        auto exit = AccessEvent( "ExitProcess", CurrentProcessID() );
        EventSignal( exit );
        EventWait( monitor );
        ReleaseEvent( monitor );
        ReleaseEvent( exit );
    }

} // namespace AccessMonitor
