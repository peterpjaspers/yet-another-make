#pragma once

#include <filesystem>
#include <boost/interprocess/detail/os_thread_functions.hpp>
#include <boost/asio.hpp>

namespace YAM
{
    class __declspec(dllexport) BuildServicePortRegistry
    {
    public:
        static std::filesystem::path servicePortRegistryPath();

        // Store pid of current process and port in registry. 
        // For use by process that hosts BuildService.
        BuildServicePortRegistry(boost::asio::ip::port_type port);

        // Read service pid and port from registry.
        // For use by process that hosts BuildClient.
        BuildServicePortRegistry();

        // Return whether registry I/O succeeded.
        bool good() const { return _good;  }

        // retry reading the registry
        void read();

        boost::interprocess::ipcdetail::OS_process_id_t pid() const { return _pid; }
        boost::asio::ip::port_type port() const { return _port; }

        // Return whether the server process is running.
        bool serverRunning() const;

    private:
        bool _good;
        boost::interprocess::ipcdetail::OS_process_id_t _pid;
        boost::asio::ip::port_type _port;
    };
}


