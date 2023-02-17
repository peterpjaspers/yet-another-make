#include "FileRepository.h"

namespace YAM
{
	FileRepository::FileRepository(
		std::string const& repoName,
		std::filesystem::path const& directory,
		RegexSet const& excludes)
		: _name(repoName)
		, _directory(directory)
		, _excludes(excludes)
	{}

	std::string const& FileRepository::name() const { 
		return _name; 
	}

	std::filesystem::path const& FileRepository::directory() const { 
		return _directory; 
	}
	RegexSet const& FileRepository::excludes() const {
		return _excludes;
	}

	bool FileRepository::contains(std::filesystem::path const& absPath) const {
		const std::size_t result = absPath.string().find(_directory.string());
		const bool contains = result == 0;
		return contains;
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
