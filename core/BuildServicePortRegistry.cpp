#include "BuildServicePortRegistry.h"
#include "DotYamDirectory.h"

#include <fstream>
#include <boost/process.hpp>

namespace YAM
{
    std::filesystem::path BuildServicePortRegistry::servicePortRegistryPath() {
        static std::filesystem::path registryPath;
        if (registryPath.empty()) {
            DotYamDirectory yamDir;
            registryPath = yamDir.dotYamDir() / ".servicePort";
        }
        return registryPath;
    }

    BuildServicePortRegistry::BuildServicePortRegistry(boost::asio::ip::port_type port)
        : _pid(boost::interprocess::ipcdetail::get_current_process_id())
        , _port(port)
    {
        std::ofstream portFile(servicePortRegistryPath().string());
        portFile << _pid << " " << _port << std::endl;
        _good = portFile.good();
        portFile.close();
    }

    BuildServicePortRegistry::BuildServicePortRegistry()
    {
        read();
    }

    void BuildServicePortRegistry::read()
    {
        std::ifstream portFile(servicePortRegistryPath().string());
        portFile >> _pid >> _port;
        _good = portFile.good();
        portFile.close();
    }

    bool BuildServicePortRegistry::serverRunning() const {
        if (!good()) return false;
        try {
            boost::process::child server(_pid);
            return server.running();
        } catch (std::system_error) {
            return false;
        }   
    }
}
