#pragma once

#include "IMonitoredProcess.h"
#include <memory>
#include <filesystem>

namespace YAM
{
    class __declspec(dllexport) MonitoredProcess : public IMonitoredProcess
    {
    public:
        MonitoredProcess(
            std::string const& program,
            std::string const& arguments,
            std::filesystem::path const& workingDir,
            std::map<std::string, std::string> const& env);

        MonitoredProcessResult const& wait() override;
        bool wait_for(unsigned int timoutInMilliSeconds) override;
        void terminate() override;

    private:
        std::shared_ptr<IMonitoredProcess> _impl;
    };
}

