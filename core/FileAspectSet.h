#pragma once

#include "FileAspect.h"

#include <vector>
#include <tuple>
#include <filesystem>

namespace YAM
{
	// A set of file aspects, no duplicate aspect names allowed.
	class __declspec(dllexport) FileAspectSet
	{
	public:
		FileAspectSet() = default;

		void add(FileAspect const& aspect);
		void remove(FileAspect const& aspect);
		void clear();

		std::vector<FileAspect> const & aspects() const;

		// Find aspect with given aspect name.
		// If found: return {true, found aspect}, else return {false, dont_use} 
		std::pair<bool, FileAspect const&> find(std::string const & aspectName) const;

		// Find the aspect applicable for given file.
		// Find aspect with given aspect name and return it.
		// Return entireFile aspect when given name is not found.
		// Throw an exception when multiple aspects are applicable.
		FileAspect const& findApplicableAspect(std::filesystem::path const & fileName) const;

	private:
		std::vector<FileAspect> _aspects;
	};
}


