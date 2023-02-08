#include "BuildRequest.h"

namespace YAM
{
	BuildRequest::BuildRequest(RequestType type)
		: _type(type)
	{}

	// Set/get the build command type.
	void BuildRequest::requestType(BuildRequest::RequestType type) {
		_type = type;
	}

	BuildRequest::RequestType BuildRequest::requestType() const {
		return _type;
	}

	// Set/get the directory from which the build is started.
	void BuildRequest::directory(std::filesystem::path const& directory) {
		_directory = directory;
	}

	std::filesystem::path const& BuildRequest::directory() {
		return _directory;
	}

	void BuildRequest::addToScope(std::filesystem::path const& path) {
		_pathsInScope.push_back(path);
	}

	std::vector<std::filesystem::path> const& BuildRequest::pathsInScope() {
		return _pathsInScope;
	}
}