#include "BuildResult.h"
#include "IStreamer.h"

#include <sstream>

namespace
{
    uint32_t _streamableType = 0;
}

namespace YAM
{
    BuildResult::BuildResult()
        : _state(State::Unknown)
        , _startTime(std::chrono::system_clock::now())
        , _nNodesStarted(0)
        , _nNodesExecuted(0)
        , _nRehashedFiles(0)
        , _nDirectoryUpdates(0)
    {}

    BuildResult::BuildResult(State state_) : BuildResult() {
        state(state_);
    }

    BuildResult::BuildResult(IStreamer* reader) {
        stream(reader);
    }

    void BuildResult::state(State newState) {
        if (_state != newState) {
            _state = newState;
            _endTime = std::chrono::system_clock::now();
        }
    }
    BuildResult::State BuildResult::state() const {
        return _state;
    }

    std::chrono::system_clock::time_point BuildResult::startTime() const {
        return _startTime;
    }

    std::chrono::system_clock::time_point BuildResult::endTime() const {
        return _endTime;
    }

    std::chrono::system_clock::duration BuildResult::duration() const {
        return _endTime - _startTime;
    }

    void BuildResult::setStreamableType(uint32_t type) {
        _streamableType = type;
    }

    std::string BuildResult::niceDuration() const {
        auto d = duration();
        auto milliSeconds = (std::chrono::duration_cast<std::chrono::milliseconds>(duration())).count();

        long long hours = milliSeconds / (60 * 60 * 1000);
        milliSeconds -= hours * 60 * 60 * 1000;
        long long minutes = milliSeconds / (60 * 1000);
        milliSeconds -= minutes * 60 * 1000;
        long long seconds = milliSeconds / 1000;
        milliSeconds -= seconds * 1000;

        std::stringstream ss;
        if (hours == 1) {
            ss << "1 hour ";
        } else if (hours > 1) {
            ss << hours << " hours ";
        }
        if (minutes == 1) {
            ss << "1 minute ";
        } else if (minutes > 1) {
            ss << minutes << " minutes ";
        }
        if (seconds == 1) {
            ss << "1 second ";
        } else if (seconds > 1) {
            ss << seconds << " seconds ";
        }
        if (milliSeconds == 1) {
            ss << "1 milliseconds ";
        } else if (milliSeconds > 1) {
            ss << milliSeconds << " milliseconds";
        }
        return ss.str();
    }

    void BuildResult::nNodesStarted(unsigned int value) { _nNodesStarted = value; }
    void BuildResult::nNodesExecuted(unsigned int value) { _nNodesExecuted = value; }
    void BuildResult::nRehashedFiles(unsigned int value) { _nRehashedFiles = value; }
    void BuildResult::nDirectoryUpdates(unsigned int value) { _nDirectoryUpdates = value; }

    unsigned int BuildResult::nNodesStarted() const { return _nNodesStarted; }
    unsigned int BuildResult::nNodesExecuted() const { return _nNodesExecuted; }
    unsigned int BuildResult::nRehashedFiles() const { return _nRehashedFiles; }
    unsigned int BuildResult::nDirectoryUpdates() const { return _nDirectoryUpdates; }

    uint32_t BuildResult::typeId() const {
        return _streamableType;
    }

    void BuildResult::stream(IStreamer* streamer) {
        uint32_t s = _state;
        streamer->stream(s);
        if (streamer->reading()) _state = static_cast<State>(s);
        streamer->stream(_startTime);
        streamer->stream(_endTime);
        streamer->stream(_nNodesStarted);
        streamer->stream(_nNodesExecuted);
        streamer->stream(_nRehashedFiles);
        streamer->stream(_nDirectoryUpdates);
    }
}


