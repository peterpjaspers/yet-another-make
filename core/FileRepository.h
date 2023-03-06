#pragma once

#include <string>
#include <filesystem>

namespace YAM
{
    // A file repository represents a directory tree in the file system. 
    // 
    // Repository name and build state comparison.
    // The file nodes in YAM's build graph are identified by absolute path 
    // names. This complicates comparing two build states because clones of
    // repositories can be stored on different paths in different builds. 
    // The repo name facilitates build state comparison because it allows 
    // absolute paths to be converted to a 'symbolic' path of form
    // <repoName>/<path relative to repo directory> 
    //
    class __declspec(dllexport) FileRepository
    {
    public:
        FileRepository(std::string const& repoName, std::filesystem::path const& directory);

        virtual ~FileRepository() {}

        std::string const& name() const;
        std::filesystem::path const& directory() const;

        // Return whether 'absPath' is a path in the repo directory tree, 
        // E.g. if repo directory = /a/b/ and path = /a/b/c/e then repo
        // lexically contains path. This is also true when /a/b/c/e does not
        // exist in the file system.         //
        bool lexicallyContains(std::filesystem::path const& absPath) const;

        // Return 'absPath' relative to repo directory.
        // Return empty path when !contains(absPath).
        // Pre: 'absPath' must be without . and .. path components.
        std::filesystem::path relativePathOf(std::filesystem::path const& absPath) const;

        // Return given 'absPath' as <repoName>/<path relative to repo directory>.
        // Return empty path when !contains(absPath).
        // Pre: 'absPath' must be without . and .. path components.
        std::filesystem::path symbolicPathOf(std::filesystem::path const& absPath) const;

        // Return whether 'absPath' in the repo is allowed to be read/write accessed.
        // Default implementation: return lexicallyContains(absPath)
        virtual bool readAllowed(std::filesystem::path const& absPath) const;
        virtual bool writeAllowed(std::filesystem::path const& absPath) const;

    protected:
        std::string _name;
        std::filesystem::path _directory;
    };
}
