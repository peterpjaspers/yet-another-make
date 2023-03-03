#include "BuildResult.h"
#include "IStreamer.h"

namespace
{
	uint32_t _streamableType = 0;
}

namespace YAM
{
	BuildResult::BuildResult() 
		: _succeeded(false)
		, _startTime(std::chrono::system_clock::now())
	{}

	void BuildResult::succeeded(bool value) {
		_succeeded = value;
		_endTime = std::chrono::system_clock::now();
	}

	bool BuildResult::succeeded() const {
		return _succeeded;
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

	uint32_t BuildResult::typeId() const {
		return _streamableType;
	}

	void BuildResult::stream(IStreamer* streamer) {
		streamer->stream(_succeeded);
		streamer->stream(_startTime);
		streamer->stream(_endTime);
	}
}


