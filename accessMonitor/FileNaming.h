#ifndef ACCESS_MONIOTR_FILE_NAMING_H
#define ACCESS_MONIOTR_FILE_NAMING_H

#include "Process.h"

#include <string>
#include <filesystem>

namespace AccessMonitor {

    std::wstring uniqueName( const std::wstring& name, unsigned long code, const std::wstring& extension = L"" );
    std::wstring uniqueName( const std::wstring& name, unsigned long code1,  unsigned long code2,const std::wstring& extension = L"" );
    std::filesystem::path sessionInfoPath( const ProcessID process );
    std::filesystem::path sessionDataPath( const SessionID session );
    std::filesystem::path monitorEventsPath( const ProcessID process, const SessionID session );

    // Test if we are executing in a remote session process.
    // Returns true if remote, else false.
    bool remoteSession();
    // Record session ID for a remote process.
    void recordSessionInfo( const SessionID session, const ProcessID process, ThreadID thread );
    // Retrieve session ID in a remote process.
    void retrieveSessionInfo( const ProcessID process, SessionID& session, ThreadID& thread );

} // namespace AccessMonitor

#endif // ACCESS_MONIOTR_FILE_NAMING_H
