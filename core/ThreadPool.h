#pragma once

#include "Dispatcher.h"
#include "Thread.h"

#include <vector>
#include <string>
#include <memory>
#include <thread>

namespace YAM
{
    class __declspec(dllexport) ThreadPool
    {
    public:
        ThreadPool(Dispatcher* dispatcher, std::string const& name, std::size_t nThreads);
        ~ThreadPool();

        // Return the number of threads in the pool.
        std::size_t size() const;

        // Adjust the number of threads in the pool. 
        // 
        // YAM only adjusts size (when the users requests a non-default size) 
        // at the start of a build, i.e. when the dispatcher queue is empty and
        // no delegate processing is in progress. Implementation is therefore 
        // kept simple: 
        //      - stop dispatcher to request all threads to stop
        //      - join and destruct all threads
        //      - start dispatcher and create new threads.
        // 
        // Stopping a thread that is busy executing a delegate will not
        // cancel the delegate. Instead the thread will run the delegate
        // to completion and then stop. Adjusting size will therefore block
        // the calling thread for the time needed to complete all delegates
        // that were executing at time of size adjustment request. 
        // 
        // In YAM delegates typically dispatch a completion delegate to the
        // main thread. Adjusting size from the main thread will block it, 
        // causing these completion delegates to remain queued on the main 
        // thread until size adjustment has completed.
        //
        void size(std::size_t newSize);

        // Join with all threads in pool.
        // Block caller until all dispatched delegates have been executed, 
        // then stop dispatcher and join with threads in pool.
        // Post: size() == 0
        void join();

    private:
        Dispatcher* _dispatcher;
        std::string _name;
        std::vector<std::shared_ptr<Thread>> _threads;
    };
}

