#include "FileAspect.h"

namespace YAM
{
	FileAspect::FileAspect(
		std::string const& name,
		RegexSet const& fileNamePatterns)
		: _name(name)
		, _fileNamePatterns(fileNamePatterns)
	{ }

	std::string const& FileAspect::name() const {
		return _name;
	}

	RegexSet & FileAspect::fileNamePatterns() {
		return _fileNamePatterns;
	}

	bool FileAspect::matches(std::filesystem::path fileName) const {
		std::string name = fileName.string();
		return _fileNamePatterns.matches(name);
	}

	FileAspect const & FileAspect::entireFileAspect() {
		static FileAspect entireFileAspect("entireFile", RegexSet({ ".*" }));
		return entireFileAspect;
	}
}