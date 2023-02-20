#include "FileRepository.h"

namespace YAM
{
	FileRepository::FileRepository(std::string const& repoName, std::filesystem::path const& directory)
		: _name(repoName)
		, _directory(directory)
	{}

	std::string const& FileRepository::name() const { 
		return _name; 
	}

	std::filesystem::path const& FileRepository::directory() const { 
		return _directory; 
	}

	bool FileRepository::lexicallyContains(std::filesystem::path const& absPath) const {
		const std::size_t result = absPath.string().find(_directory.string());
		return result == 0;
	}

    bool FileRepository::readAllowed(std::filesystem::path const& absPath) const {
		return lexicallyContains(absPath);
	}

	bool FileRepository::writeAllowed(std::filesystem::path const& absPath) const {
		return lexicallyContains(absPath);
	}

	std::filesystem::path FileRepository::relativePathOf(std::filesystem::path const& absPath) const {
		std::filesystem::path relPath;
		const std::string absStr = absPath.string();
		const std::string dirStr = (_directory / "").string();
		const std::size_t result = absStr.find(dirStr);
		if (result == 0) {
			relPath = absStr.substr(result + dirStr.length(), absStr.length() - dirStr.length());
		}
		return relPath;
	}

	std::filesystem::path FileRepository::symbolicPathOf(std::filesystem::path const& absPath) const {
		auto relPath = relativePathOf(absPath);
		if (relPath.empty()) return relPath;
		return std::filesystem::path(_name) / relPath;
	}
}
