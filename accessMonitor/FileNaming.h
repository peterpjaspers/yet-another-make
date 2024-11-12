#ifndef ACCESS_MONIOTR_FILE_NAMING_H
#define ACCESS_MONIOTR_FILE_NAMING_H

#include "Process.h"

#include <string>
#include <filesystem>

namespace AccessMonitor {

    std::wstring const dataDirectory();
    std::wstring uniqueName( const std::wstring& name, unsigned long code, const std::wstring& extension = L"" );
    std::wstring uniqueName( const std::wstring& name, unsigned long code1,  unsigned long code2,const std::wstring& extension = L"" );
    std::filesystem::path sessionDataPath(const std::filesystem::path& dir, const SessionID session );
    std::filesystem::path monitorDebugPath( const std::filesystem::path& dir, const ProcessID process, const SessionID session );
    std::filesystem::path monitorEventsPath(const std::filesystem::path& dir, const ProcessID process, const SessionID session );

} // namespace AccessMonitor

#endif // ACCESS_MONIOTR_FILE_NAMING_H
