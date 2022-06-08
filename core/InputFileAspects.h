#pragma once

#include "FileAspectSet.h"
#include <filesystem>
#include <string>
#include <regex>

namespace YAM
{
	// Given an output file that is computed from a set of input files. In some
	// cases not all content of an input file is relevant for the resulting 
	// output file. E.g. for an .obj file that is compiled from C++ input files
	// the comments in those files do not contribute to the .obj output.
	// 
	// InputFileAspects defines, for a given output file type, which input file 
	// aspects are relevant for the computation of a given output file type.
	//
	class __declspec(dllexport) InputFileAspects
	{
	public:
		// Construct an object that defines that given inputAspects contribute 
		// to output files whose name match given outputFileNamePattern.
		InputFileAspects(
			std::string const& outputFileNamePattern,
			FileAspectSet const & inputAspects);

		bool applicableForOutput(std::filesystem::path const& outputFileName) const;

		FileAspectSet const& inputAspects() const;

	private:
		std::string _outputFileNamePattern;
		std::regex _outputFileRegex;
		FileAspectSet _inputAspects;
	};
}

