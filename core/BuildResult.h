#pragma 

#include "IStreamable.h"
#include <chrono>
#include <string>

namespace YAM
{
    class __declspec(dllexport) BuildResult : public IStreamable
    {
    public:
        BuildResult();
        BuildResult(bool success);
        BuildResult(IStreamer* reader);

        void succeeded(bool value);
        bool succeeded() const;

        // Return time of BuildResult construction
        std::chrono::system_clock::time_point startTime() const;
        // Return time of call to succeeded(value)
        std::chrono::system_clock::time_point endTime() const;
        // Return endTime() - startTime()
        std::chrono::system_clock::duration duration() const;
        // Return duration as string: h hours m minute etc.
        std::string niceDuration() const;

        void nNodesStarted(unsigned int);
        void nNodesExecuted(unsigned int);
        void nRehashedFiles(unsigned int);
        void nDirectoryUpdates(unsigned int);

        unsigned int nNodesStarted() const;
        unsigned int nNodesExecuted() const;
        unsigned int nRehashedFiles() const;
        unsigned int nDirectoryUpdates() const;

        static void setStreamableType(uint32_t type);
        // Inherited via IStreamable
        uint32_t typeId() const override;
        void stream(IStreamer* streamer) override;

    private:
        bool _succeeded;
        std::chrono::system_clock::time_point _startTime;
        std::chrono::system_clock::time_point _endTime;

        unsigned int _nNodesStarted;
        unsigned int _nNodesExecuted;
        unsigned int _nRehashedFiles;
        unsigned int _nDirectoryUpdates;
    };
}


