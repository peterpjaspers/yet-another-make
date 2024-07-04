#ifndef ACCESS_MONIOTR_FILE_NAMING_H
#define ACCESS_MONIOTR_FILE_NAMING_H

#include "Process.h"

#include <string>
#include <filesystem>

namespace AccessMonitor {

    std::filesystem::path const& dataDirectory();
    std::wstring uniqueName( const std::wstring& name, unsigned long code, const std::wstring& extension = L"" );
    std::wstring uniqueName( const std::wstring& name, unsigned long code1,  unsigned long code2,const std::wstring& extension = L"" );
    std::filesystem::path sessionInfoPath(std::filesystem::path const& sessionDir, const ProcessID process );
    std::filesystem::path sessionDataPath(std::filesystem::path const& sessionDir, const SessionID session );
    std::filesystem::path monitorEventsPath(std::filesystem::path const& sessionDir, const ProcessID process, const SessionID session );

    // Record session ID for a remote process.
    void recordSessionInfo(std::filesystem::path const& sessionDir, const SessionID session, const ProcessID process, ThreadID thread );
    // Retrieve session ID in a remote process.
    void retrieveSessionInfo(std::filesystem::path const& sessionDir, const ProcessID process, SessionID& session, ThreadID& thread );

} // namespace AccessMonitor

#endif // ACCESS_MONIOTR_FILE_NAMING_H
