#pragma once

#include "FileAspectSet.h"
#include <filesystem>
#include <vector>
#include <string>
#include <regex>
#include <tuple>

namespace YAM
{
	// Given an output file that is computed from a set of input files. In some
	// cases not all content of an input file is relevant for the resulting 
	// output file. E.g. for an .obj file that is compiled from C++ input files
	// the comments in those files do not contribute to the .obj output.
	// 
	// InputFileAspects defines, for a given output file type, which input file 
	// aspects are relevant for the computation of that output file.
	//
	// Transform OutputToInput
	class __declspec(dllexport) InputFileAspects
	{
	public:
		InputFileAspects() = default;

		// Construct an object that stores inputAspects as the relevant aspects
		// for output files whose names match given regex pattern.
		InputFileAspects(
			std::string const& outputFileNamePattern,
			FileAspectSet const & inputAspects);

		std::string const& outputFileNamePattern() const;
		FileAspectSet & inputAspects();

		// Return whether outputFileName matches outputFileNamePattern().
		bool matches(std::filesystem::path const& outputFileName) const;

	private:

		std::string _outputFileNamePattern;
		std::regex _outputFileRegex;
		FileAspectSet _inputAspects;
	};

	class __declspec(dllexport) InputFileAspectsSet
	{
	public:
		InputFileAspectsSet();

		// Add given inputfile aspects to the set.
		// Return whether newAspects was added. newAspects cannot be added when
		// newAspects.outputFileNamePattern() already exists in the set.
		// Note: output filename patterns must be such that no two patterns
		// will match the same output file name, see findOutputMatch().
		bool add(InputFileAspects const& newAspects);

		// Remove input aspects set with given output pattern from the set.
		// Return whether input aspects set was found and removed.
		bool remove(std::string const& outputFileNamePattern);

		bool contains(std::string const& outputFileNamePattern) const;

		void clear();

		// Find the InputFileAspects that has given output filename pattern. 
		// Return true and found aspects, return false otherwise.
	    std::pair<bool, InputFileAspects const&> find(std::string const & outputFileNamePattern) const;

		// Find the InputFileAspects whose output file pattern matches the given 
		// output file name. Return the found aspects, else return a InputFileAspects
		// that select FileAspect::entireFile() for all input files.
		// Throw an exception when multiple patterns match the given output file.
		InputFileAspects const& findOutputMatch(std::filesystem::path const & outputFileName) const;

	private:
		InputFileAspects _entireFileForAll;
		std::vector<InputFileAspects> _inputFileAspects;
	};
}

