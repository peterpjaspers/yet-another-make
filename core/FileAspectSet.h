#pragma once

#include "FileAspect.h"

#include <string>
#include <map>
#include <vector>
#include <tuple>
#include <filesystem>

namespace YAM
{
    // A set of file aspects.
    // Duplicate aspect names are not allowed.
    class __declspec(dllexport) FileAspectSet
    {
    public:
        FileAspectSet() = default;
        FileAspectSet(std::string const & name);

        std::string const & name() const;

        // Add given aspect to the set.
        // Throw an exception when aspect.name() already in set.
        void add(FileAspect const& aspect);
        void remove(FileAspect const& aspect);
        bool contains(std::string const& aspectName) const;
        void clear();

        // Return the file aspects in the set ordered by
        // ascending aspect name.
        std::vector<FileAspect> aspects() const;

        // Find aspect with given aspect name.
        // If found: return {true, found aspect}, else return {false, dont_use} 
        std::pair<bool, FileAspect const&> find(std::string const & aspectName) const;

        // Find the aspect that is applicable for the file with the given file name.
        // Return entireFile aspect when given name is not found.
        FileAspect const& findApplicableAspect(std::filesystem::path const & fileName) const;

        // Return file aspect set containing only FileAspect::entireFileAspect.
        static FileAspectSet const& entireFileSet();

    private:
        std::string _name;
        std::map<std::string, FileAspect> _aspects;
    };
}


