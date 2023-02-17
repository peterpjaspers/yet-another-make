#pragma once

#include "RegexSet.h"

#include <string>
#include <filesystem>

namespace YAM
{
	// A file repository represents a directory tree in the file system that
	// contains directories and files that are allowed to be accessed by YAM. 
	// Examples of files in a repository are: YAM build files, source files 
	// like .cpp (C++) and .cs (C# ) files, generated files like object files, 
	// libraries and executables. 
	// YAM will fail a build when it detects access to a directory or file that
	// is not in one of the configured file repositories. 
	// 
	// Build state comparison.
	// The file nodes in YAM's build graph are identified by absolute path 
	// names. This complicates comparing two build states because (clones of)
	// repositories can be stored on different paths in different builds. 
	// The repo name facilitates build state comparison because it allows 
	// absolute paths to be converted to a 'symbolic' path of form
	// <repoName>/<path relative to repo directory> 
	//
	class __declspec(dllexport) FileRepository
	{
	public:
		// Construct a repository.
		// A directories or file with path P in the repository is allowed to
		// be accessed when the following is true:
		// excludeHasPriority
		//     ? includes.matches(P) && !excludes.matches(P))
		//     : excludes.matches(P) && includes.matches(P)
		//
		FileRepository(
			std::string const& repoName,
			std::filesystem::path const& directory,
		    RegexSet const& excludes = RegexSet());

		virtual ~FileRepository() {}

		std::string const& name() const;
		std::filesystem::path const& directory() const;

		// TODO: to be removed once .gitignore is supported
		RegexSet const& excludes() const;
		virtual void excludes(RegexSet const& newExcludes) { _excludes = newExcludes; }

		// Return whether 'absPath' is a path in the repo directory tree, 
		// E.g. if repo directory() = /a/b/ and path = /a/b/c/e then
		// repo contains path. This is also true when /a/b/c/e does not
		// exist in the file system or when it was excluded by the
		// exclude patterns. 
		// Pre: 'absPath' must be without . and .. path components.
		// TODO?: excluded paths must return false? This requires parsing the
		// .ignore files applicable to the directory that contains path.
		// For SourceFileRepository this is integrated in DirectoryNode.
		// Duplicating this in FileRepository seems clumsy while blindly
		// mirroring all repos seems inefficient.
		bool contains(std::filesystem::path const& absPath) const;

		// Return 'absPath' relative to repo directory.
		// Return empty path when !contains(absPath).
		// Pre: 'absPath' must be without . and .. path components.
		std::filesystem::path relativePathOf(std::filesystem::path const& absPath) const;

		// Return given 'absPath' as <repoName>/<path relative to repo directory>.
		// Return empty path when !contains(absPath).
		// Pre: 'absPath' must be without . and .. path components.
		std::filesystem::path symbolicPathOf(std::filesystem::path const& absPath) const;

		// Return whether files in the repo are allowed to be read and/or write
		// accessed.
		virtual bool readOnly() const { return false; }
		virtual bool writeOnly() const { return false; }
		virtual bool readWrite() const { return true; }

	protected:
		std::string _name;
		std::filesystem::path _directory;
		RegexSet _excludes;
	};
}
