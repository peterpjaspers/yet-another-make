#include "BuildResult.h"

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
}


