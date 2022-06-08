#pragma once

#include "FileAspect.h"

#include <vector>
#include <filesystem>

namespace YAM
{
	// A set of file aspects, no duplicate aspect names allowed.
	class __declspec(dllexport) FileAspectSet
	{
	public:
		FileAspectSet() = default;

		std::vector<FileAspect> const & aspects() const;

		// Find aspect with given name. Return nullptr when not found.
		FileAspect const* find(std::string const & aspectName) const;

		// Find the aspect applicable for given file.
		// Return nullptr when not found.
		// Throw an exception when multiple aspects are applicable.
		FileAspect const * findApplicableAspect(std::filesystem::path const & fileName) const;

		// Find the name of the aspect applicable for given file.
		// When not found return "entireFile".
		// Throw an exception when multiple aspects are applicable.
		std::string const& findApplicableAspectName(std::filesystem::path const& inputFileName) const;

		void add(FileAspect const & aspect);
		void remove(FileAspect const & aspect);
		void clear();

	private:
		std::vector<FileAspect> _aspects;
	};
}


