#pragma once

#include "RegexSet.h"

#include <string>
#include <vector>
#include <filesystem>
#include <regex>

namespace YAM
{
	class __declspec(dllexport) FileAspect
	{
	public:
		// Construct an object that identifies a file aspect by given 
		// aspect name. The aspect is applicable for files whose names
		// match the given filename patterns.
		// An example of a file aspect is the code aspect of a C++ file,
		// i.e. all parts of the file excluding comment sections.
		// C++ filename patterns include .cpp, .h, .hpp, .inline.
		FileAspect(
			std::string const& aspectName,
			RegexSet const & fileNamePatterns);

		std::string const& aspectName() const;
		RegexSet const& fileNamePatterns() const;

		bool applicableFor(std::filesystem::path fileName) const;

	private:
		std::string _aspectName;
		RegexSet _fileNamePatterns;
	};
}
