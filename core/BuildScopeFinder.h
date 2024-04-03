#pragma once

#include "BuildOptions.h"
#include "Glob.h"

#include <vector>
#include <filesystem>

namespace YAM
{
    class ExecutionContext;
    class Node;
    class CommandNode;
    class GeneratedFileNode;

    class __declspec(dllexport) BuildScopeFinder
    {
    public:
        BuildScopeFinder(ExecutionContext* context, BuildOptions const& options);

        // Return the dirty command and foreach nodes that have output files
        // that match option._scope.
        std::vector<std::shared_ptr<Node>> dirtyCommands()const;

        // Return the generated file nodes (dirty and not-dirty) that match
        // option._scope.
        std::vector<std::shared_ptr<GeneratedFileNode>> generatedFiles() const;

        // Return whether given cmd is Dirty and has ouput files that match 
        // option._scope.
        bool inScope(std::shared_ptr<CommandNode> const& cmd) const;

    private:
        ExecutionContext* _context;
        std::vector<std::filesystem::path> _paths;
        std::vector<Glob> _globs;
    };
}

