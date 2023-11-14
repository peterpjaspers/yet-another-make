#include "MonitoredProcess.h"

#if defined( _WIN32 )
#include "MonitoredProcessWin32.h"
#define MP_IMPL_CLASS MonitoredProcessWin32
#elif
#error "platform is not supported"
#endif

namespace YAM
{
    MonitoredProcess::MonitoredProcess(
        std::string const& program,
        std::string const& arguments,
        std::filesystem::path const& workingDir,
        std::map<std::string, std::string> const& env)
        : IMonitoredProcess(program, arguments, workingDir, env)
    {
        _impl = std::make_shared<MP_IMPL_CLASS>(_program, _arguments, _workingDir, _env);
    }

    MonitoredProcessResult const& MonitoredProcess::wait() {
        return _impl->wait();
    }

    bool MonitoredProcess::wait_for(unsigned int timoutInMilliSeconds) {
        return _impl->wait_for(timoutInMilliSeconds);
    }

    void MonitoredProcess::terminate() {
        return _impl->terminate();
    }
}