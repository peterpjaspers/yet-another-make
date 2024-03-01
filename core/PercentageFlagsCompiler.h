#pragma once

#include "BuildFile.h"

#include <filesystem>
#include <memory>
#include <vector>
#include <string>

namespace YAM
{
    class ExecutionContext;
    class Node;
    class GeneratedFileNode;
    class DirectoryNode;

    class __declspec(dllexport) PercentageFlagsCompiler
    {
    public:
        // Return whether the contains input and/or output flags.
        static bool containsFlags(std::string const& str);

        // Output path compilation: compiles the output section of a build rule
        // Result will be the output file path: relative if outut is in home
        // repository, absolute path otherwise.
        PercentageFlagsCompiler(
            std::filesystem::path const& buildFile,
            BuildFile::Output const &output,
            ExecutionContext* context,
            std::shared_ptr<DirectoryNode> const &baseDir,
            std::vector<std::shared_ptr<Node>> cmdInputs,
            std::size_t defaultInputOffset);

        // Build script compilation: compiles the command script section of a
        // build rule. Result will be the script with references to input and 
        // output paths expanded. Paths in home repository will be relative to
        // baseDir, absolute paths otherwise.  
        PercentageFlagsCompiler(
            std::filesystem::path const& buildFile,
            BuildFile::Script const &script,
            std::shared_ptr<DirectoryNode> const& baseDir,
            std::vector<std::shared_ptr<Node>> cmdInputs,
            std::vector<std::shared_ptr<Node>> orderOnlyInputs,
            std::vector<std::shared_ptr<GeneratedFileNode>> const& outputs);

        std::string result() const { return _result; }

    private:
        std::string _result;
    };
}

