#include "StopBuildRequest.h"

namespace
{
	uint32_t _streamableType = 0;
}
namespace YAM
{
	void StopBuildRequest::setStreamableType(uint32_t type) {
		_streamableType = type;
	}

	uint32_t StopBuildRequest::typeId() const {
		return _streamableType;
	}

	void StopBuildRequest::stream(IStreamer* streamer) {	}
}
