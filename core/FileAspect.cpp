#include "FileAspect.h"

namespace YAM
{
	FileAspect::FileAspect(
		std::string const& aspectName,
		RegexSet const& fileNamePatterns)
		: _aspectName(aspectName)
		, _fileNamePatterns(fileNamePatterns)
	{ }

	std::string const& FileAspect::aspectName() const {
		return _aspectName;
	}

	RegexSet const& FileAspect::fileNamePatterns() const {
		return _fileNamePatterns;
	}

	// Return whether this aspect is applicable for the given file name.
	// The aspect is applicable when the file name matches one of the
	// file name patterns.
	bool FileAspect::applicableFor(std::filesystem::path fileName) const {
		std::string name = fileName.string();
		return _fileNamePatterns.matches(name);
	}
}