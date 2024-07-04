#include "FileNaming.h"

#include <fstream>

using namespace std;
using namespace std::filesystem;

namespace AccessMonitor {


    namespace {
        const path _dataDirectory(L"AccessMonitorData");
    }

    path const& dataDirectory() { return _dataDirectory; }

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

    // Return path to session ID file
    path sessionInfoPath(path const& sessionDir, const ProcessID process) {
        return path(sessionDir / _dataDirectory / uniqueName( L"SessionID", process, L"txt" ) );
    }
    // Return path to session data directory
    path sessionDataPath(path const& sessionDir, const SessionID session) {
        return path(sessionDir / _dataDirectory / uniqueName( L"Session", session ) );
    }
    // Return path to monitor events for this process
    path monitorEventsPath(path const& sessionDir, const ProcessID process, const SessionID session) {
        return sessionDataPath( sessionDir, session ) / uniqueName( L"Monitor_Events", process, L"log" );
    }
    // Record session ID and main thread ID of a (remote) process
    void recordSessionInfo(path const& sessionDir, const SessionID session, const ProcessID process, ThreadID thread ) {
        auto sessionIDFile = ofstream( sessionInfoPath( sessionDir, process ) );
        sessionIDFile << session << " " << thread << endl;
        sessionIDFile.close();
    }
    // Retrieve session ID and main thread ID of a (remote) process
    void retrieveSessionInfo(path const& sessionDir, const ProcessID process, SessionID& session, ThreadID& thread ) {
        auto sessionFile = ifstream( sessionInfoPath( sessionDir, process ) );
        sessionFile >> session >> thread;
        sessionFile.close();            
    }

} // namespace AccessMonitor
