#pragma once
#include "Node.h"
#include "../xxHash/xxhash.h"

#include <filesystem>
#include <set>
#include <memory>

namespace YAM
{
    class ExecutionContext;
    class DirectoryNode;

    // When executing a GlobNode it applies its glob pattern to the mirrored 
    // repositories, i.e. to the DirectoryNodes and FileNodes in the execution
    // context. It provides access to the matching nodes. 
    // 
    // A glob pattern specifies a sets of filenames with wildcard characters.
    // E.g *.txt specifies all text files in the base directory.
    //
    // A glob pattern can contain the following wildcards:
    //    *  matches any number of any characters including none 
    //    ?  matches any single character
    //   **  matches all files and 0 or more directories and subdirectories. 
    //       If the pattern is followed by a /, only directories and 
    //       subdirectories match.
    // 
    // Globs do not match invisible files like ., .., .git, .rc unless the
    // dot (.) is specified in the glob pattern.
    // 
    // Glob examples:
    //    *.cpp matches all visible cpp files in the anchor directory
    //    .* matches all visible and invisible files in the anchor directory
    //    /root/cor*/*.cpp matches all cpp files in directories whose names 
    //    match /root/cor* 
    //    /root/**/*.cpp matches all cpp files in all directory trees in /root
    // 
    // When executing a GlobNode it applies the glob pattern to the mirrored 
    // repositories, i.e. to the DirectoryNodes and FileNodes in the execution
    // context.
    //
    class __declspec(dllexport) GlobNode : public Node
    {
        GlobNode(); // needed for deserialization
        GlobNode(ExecutionContext* context, std::filesystem::path const& name);

        void start() override;

        // Return a hash of the names of the nodes that match the glob pattern.
        // Nodes that use these names as inputs must re-execute when this hash
        // changes.
        XXH64_hash_t hash();

        static void setStreamableType(uint32_t type);
        // Inherited from IStreamable
        uint32_t typeId() const override;
        void stream(IStreamer* streamer) override;

    private:
        std::vector<std::shared_ptr<Node>> find(
            std::filesystem::path const& inpath,
            bool dirsOnly);

        XXH64_hash_t executionHash();

        std::shared_ptr<DirectoryNode> _anchor;
        std::filesystem::path const& _pattern;

        std::set<std::shared_ptr<DirectoryNode>> _inputs;
        std::set<std::shared_ptr<Node>> _matches;
        XXH64_hash_t _executionHash;
        XXH64_hash_t _hash;
    };
}

