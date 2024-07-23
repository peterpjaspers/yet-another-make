#pragma once

#include <memory>
#include <boost/asio.hpp>

namespace YAM
{
    class IStreamable;
    class IInputStream;
    class IOutputStream;
    class ObjectWriter;
    class ObjectReader;

    // Class that allows for communication between build client and build
    // service and and that verifies (to some extent) that client and service
    // adhere to the build service communication protocol as described below.
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
    //        +----ShutDownRequest-------->  Build Service shuts down after 1 second   
    //
    class __declspec(dllexport) BuildServiceProtocol
    {
    public:
        // If client: construct protocol from client to service.
        // If !client: construct protocol from service to client.
        BuildServiceProtocol(
            std::shared_ptr<IInputStream> istream,
            std::shared_ptr<IOutputStream> ostream,
            bool client);

        // send and receive will throw std::exception in case of protocol
        // violation or end-of-stream (i.e. peer has closed tcp stream).
        void send(std::shared_ptr<IStreamable> message);
        std::shared_ptr<IStreamable> receive();

    private:
        std::shared_ptr<IInputStream> _istream;
        std::shared_ptr<IOutputStream> _ostream;
        std::shared_ptr<ObjectWriter> _msgSerializer;
        std::shared_ptr<ObjectReader> _msgDeserializer;
    };
}

