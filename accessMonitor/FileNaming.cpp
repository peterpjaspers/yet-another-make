#include "FileNaming.h"

#include <fstream>

using namespace std;
using namespace std::filesystem;

namespace AccessMonitor {


    namespace {
        static const wchar_t* dataDirectory = L"AccessMonitorData";
    }

    wstring uniqueName( const wstring& name, unsigned long code, const wstring& extension ) {
        wstringstream unique;
        unique << name << L"_" << hex << code;
        if (0 < extension.size()) unique << L"." << extension;
        return unique.str();
    }
    std::wstring uniqueName( const std::wstring& name, unsigned long code1,  unsigned long code2,const std::wstring& extension ) {
        wstringstream unique;
        unique << name << L"_" << hex << code1 << L"_" << hex << code2;
        if (0 < extension.size()) unique << L"." << extension;
        return unique.str();
    }

    // Return path to session ID file
    path sessionInfoPath( const ProcessID process ) {
        return path( temp_directory_path() / dataDirectory / uniqueName( L"SessionID", process, L"txt" ) );
    }
    // Return path to session data directory
    path sessionDataPath( const SessionID session ) {
        return path( temp_directory_path() / dataDirectory / uniqueName( L"Session", session ) );
    }
    // Return path to monitor events for this process
    path monitorEventsPath( const ProcessID process ) {
        return sessionDataPath( CurrentSessionID() ) / uniqueName( L"Monitor_Events", process, L"log" );
    }
    bool remoteSession() { return exists( sessionInfoPath( CurrentProcessID() ) ); }
    // Record session ID and main thread ID of a (remote) process
    void recordSessionInfo( const SessionID session, const ProcessID process, ThreadID thread ) {
        auto sessionIDFile = ofstream( sessionInfoPath( process ) );
        sessionIDFile << "0x" << hex << session << " 0x" << hex << thread << endl;
        sessionIDFile.close();
    }
    // Retrieve session ID and main thread ID of a (remote) process
    void retrieveSessionInfo( const ProcessID process, SessionID& session, ThreadID& thread ) {
        auto sessionFile = ifstream( sessionInfoPath( process ) );
        sessionFile >> hex >> session >> hex >> thread;
        sessionFile.close();            
    }

} // namespace AccessMonitor
