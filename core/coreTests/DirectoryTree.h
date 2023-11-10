#pragma once
#include "../RegexSet.h"
#include "../../xxhash/xxhash.h"

#include <filesystem>
#include <vector>
#include <fstream>
#include <cstdio>

namespace YAM
{
    class DirectoryNode;
}
namespace YAMTest
{
    class DirectoryTree
    {
    public:
        // Create in the filesystem a directory with path 'dirName'. Fill the 
        // directory with File1..3 and sub directories SubDir1..3. Recursively 
        // repeat filling the subDirs to arrive at a tree that is 'nLevels' deep. 
        // Construct a DirectoryTree that provides access to the created directory.
        // Exclude directory entries whose path name matches 'excludes' in functions
        // getFiles, getSubDirs and getHash.
        DirectoryTree(
            std::filesystem::path const& dirName, 
            unsigned int nLevels,
            YAM::RegexSet const& excludes);
        
        // Delete the directory recursively from the filesystem.
        ~DirectoryTree();

        void addFile();
        void addDirectory();
        void modifyFile(std::string const& fileName); // relative to dir
        void deleteFile(std::string const& fileName); // relative to dir
        void renameFile(std::string const& fileName, std::string const& newFileName); // relative to dir

        std::filesystem::path const& path() const;
        unsigned int nLevels() const;

        std::vector<std::filesystem::path> getFiles() const;
        std::vector<DirectoryTree*> getSubDirs() const;

        XXH64_hash_t getHash() const;

    private:
        void updateHash();

        std::filesystem::path _path;
        unsigned int _nLevels; 
        YAM::RegexSet _excludes;
        std::vector<std::filesystem::path> _files;
        std::vector<DirectoryTree*> _subDirs;
        XXH64_hash_t _hash;
    };


    // Verify that 'actual' correctly mirrors 'expected'.
    void verify(DirectoryTree* expected, YAM::DirectoryNode* actual);
}
