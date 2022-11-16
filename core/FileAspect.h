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

		FileAspect() = default;
		FileAspect(const FileAspect& other) = default;

		// Construct an object that identifies a file aspect by given 
		// aspect name. The aspect is applicable for files whose names
		// match one of the patterns in the given RegexSet.
		// An example of a file aspect is the code aspect of a C++ file,
		// i.e. all parts of the file excluding comment sections, empty 
		// lines, trailing and leading whitespace.
		// C++ filename regexes are: \.cpp$, \.h$, \.hpp$, \.inline$
		//
		FileAspect(
			std::string const& name,
			RegexSet const & fileNamePatterns);

		std::string const& name() const;
		RegexSet & fileNamePatterns();

		// Return whether given fileName matches one of the fileNamePatterns().
		bool matches(std::filesystem::path fileName) const;

		// Return the entire file aspect, i.e. the aspect that
		// includes all of a file's content and that matches all
		// file names.
		static FileAspect const & entireFileAspect();

	private:
		std::string _name;
		RegexSet _fileNamePatterns;
	};
}
