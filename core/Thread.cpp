#include "Thread.h"
#include "PriorityDispatcher.h"
#include <Windows.h>

namespace
{
    void run(YAM::PriorityDispatcher* dispatcher) {
        dispatcher->run();
    }

        namespace {

        const DWORD MS_VC_EXCEPTION = 0x406D1388;

#pragma pack(push,8)
        typedef struct tagTHREADNAME_INFO
        {
            DWORD dwType; // Must be 0x1000.
            LPCSTR szName; // Pointer to name (in user addr space).
            DWORD dwThreadID; // Thread ID (-1=caller thread).
            DWORD dwFlags; // Reserved for future use, must be zero.
        } THREADNAME_INFO;
#pragma pack(pop)


        void SetThreadName(uint32_t dwThreadID, const char* threadName)
        {

            // DWORD dwThreadID = ::GetThreadId( static_cast<HANDLE>( t.native_handle() ) );

            THREADNAME_INFO info;
            info.dwType = 0x1000;
            info.szName = threadName;
            info.dwThreadID = dwThreadID;
            info.dwFlags = 0;

            __try
            {
                RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }
        void SetThreadName(const char* threadName)
        {
            SetThreadName(GetCurrentThreadId(), threadName);
        }

        void SetThreadName(std::thread* thread, const char* threadName)
        {
            DWORD threadId = ::GetThreadId(static_cast<HANDLE>(thread->native_handle()));
            SetThreadName(threadId, threadName);
        }

    }
}

namespace YAM
{
    Thread::Thread(PriorityDispatcher* dispatcher, std::string const& name)
        : _dispatcher(dispatcher)
        , _name(name)
        , _thread(&run, _dispatcher)
    {
        SetThreadName(&_thread, name.c_str());
    }

    Thread::~Thread() {
        if (joinable()) {
            join();
        }
    }

    std::string const& Thread::name() const {
        return _name;
    }

    PriorityDispatcher* Thread::dispatcher() const {
        return _dispatcher;
    }

    bool Thread::joinable() {
        return _thread.joinable();
    }

    void Thread::join() {
        _thread.join();
    }

    bool Thread::isThisThread() const {
        auto id = _thread.get_id();
        auto tid = std::this_thread::get_id();
        return id == tid;
    }
}

