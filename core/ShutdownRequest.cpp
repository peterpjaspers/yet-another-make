#include "ShutdownRequest.h"

namespace
{
	uint32_t _streamableType = 0;
}
namespace YAM
{
	void ShutdownRequest::setStreamableType(uint32_t type) {
		_streamableType = type;
	}

	uint32_t ShutdownRequest::typeId() const {
		return _streamableType;
	}

	void ShutdownRequest::stream(IStreamer* streamer) {	}
}
