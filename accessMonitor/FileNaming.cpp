#include "FileNaming.h"

#include <fstream>
#include <windows.h>

using namespace std;
using namespace std::filesystem;

namespace AccessMonitor {

    wstring const dataDirectory() { return L"AccessMonitorData"; }

    wstring uniqueName( const wstring& name, unsigned long code, const wstring& extension ) {
        wstringstream unique;
        unique << name << L"_" << code;
        if (0 < extension.size()) unique << L"." << extension;
        return unique.str();
    }
    wstring uniqueName( const wstring& name, unsigned long code1,  unsigned long code2,const wstring& extension ) {
        wstringstream unique;
        unique << name << L"_" << code1 << L"_" << code2;
        if (0 < extension.size()) unique << L"." << extension;
        return unique.str();
    }

    // Return path to session data directory
    path sessionDataPath( const path& dir, const SessionID session ) {
        return path( dir ) / dataDirectory() / uniqueName( L"Session", session );
    }
    // Return path to access monitor debug log for this process
    path monitorDebugPath( const path& dir, const ProcessID process, const SessionID session ) {
        return path( dir ) / uniqueName( L"Monitor_Debug", process, session, L"log" );
    }
    // Return path to monitored file access events for this process
    path monitorEventsPath( const path& dir, const ProcessID process, const SessionID session ) {
        return sessionDataPath( dir, session ) / uniqueName( L"Monitor_Events", process, L"log" );
    }

} // namespace AccessMonitor
