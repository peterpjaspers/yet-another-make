#pragma once

#include <filesystem>

namespace YAM
{
    class ILogBook;

    // Parses and generates versioned buildstate file names.
    // File name format: buildstate_<writeVersion>.bt where writeVersion is 
    // the version of the buildstate stored in the file.
    class BuildStateVersion
    {
    public:
        // Set/get the version of the buildstate to be stored in the buildstate
        // file by the current version of yam. 
        static void writeVersion(uint32_t version);
        static uint32_t writeVersion();

        // Set/get the versions of the buildstate file that can be read by the
        // current version of yam.
        static void readableVersions(std::vector<uint32_t> const& versions);
        static std::vector<uint32_t> const& readableVersions();

        // Scan the directory for buildstate files.
        // If buildstate file for writeVersion() is found: return file.
        // Else: find the buildstate file with the highest version in 
        // readableVersions(). If found then copy file (upgrade it) to file
        // with buildstate file path for writeVersion() and return upgraded 
        // file. Log upgrade in logBook.
        // Else: return empty and log reason in logBook. In this case user
        // must delete build file and all previously generated (now stale)
        // output files.
        static std::filesystem::path select(
            std::filesystem::path const& directory,
            ILogBook &logBook);
    };
}

