#pragma once

#include "Dispatcher.h"
#include "ILogBook.h"
#include "Builder.h"

#include <memory>
#include <atomic>
#include <thread>

namespace YAM
{
    class TcpStream;
    class BuildResult;
    class IStreamable;

    // A build service accepts a tcp/ip connection from the build client. 
    // Only one client at-a-time can connect.
    // Client-service communication protocol:
    // 
    // Client runs build to completion:
    //     Client                       Service
    //        +----TCP Connect------------>
    //        +----BuildRequest----------->  Build is started                       
    //        <----LogRecord--------------+  0 or more times, as build progresses
    //        <----BuildResult------------+  Build has completed
    //        +----TCP Disconnect--------->  no-op
    //
    // Client stops build:
    //     Client                       Service
    //        +----TCP Connect------------->
    //        +----BuildRequest------------>  Build is started                       
    //        <----LogRecord---------------+  0 or more times, as build progresses
    //        +----BuildStopRequest-------->  Stop build is started
    //        <----LogRecord---------------+  0 or more times, as stop build progresses
    //        <----BuildResult-------------+  Build has completed
    //        +----TCP Disconnect---------->  no-op
    // 
    // Client crashes:
    //     Client                       Service
    //        +----TCP Connect------------->
    //        +----BuildRequest------------>  Build is started                       
    //        <----LogRecord---------------+  0 or more times, as build progresses
    //        +----TCP Disconnect---------->  Stop build is started
    // 
    // Client requests server to shutdown:
    //     Client                       Service
    //        +----TCP Connect------------>
    //        +----ShutDownRequest-------->  Build Service will shutdown    
    //
	class __declspec(dllexport) BuildService : 
        public ILogBook, 
        public std::enable_shared_from_this<BuildService>
	{
	public:
		BuildService();

        // Inherited via ILogBook
        void add(LogRecord const& record) override;

    private:
        void run();
        void handleRequest(std::shared_ptr<IStreamable> request);
        void handleBuildCompletion(std::shared_ptr<BuildResult> result);

        Builder _builder;
        std::atomic<bool> _shutdown;
        std::thread _thread;
        std::shared_ptr<TcpStream> _client;

    };
}

