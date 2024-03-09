#pragma once

#include "BuildOptions.h"

#include <vector>
#include <filesystem>

namespace YAM
{
    class ExecutionContext;
    class CommandNode;
    class GeneratedFileNode;

    class __declspec(dllexport) BuildScopeFinder
    {
    public:
        // Return the dirty command nodes that have output files that match 
        // option._scope.
        std::vector<std::shared_ptr<CommandNode>> findDirtyCommands(
            ExecutionContext* context,
            BuildOptions const& options
        ) const;

        // Return the generated file nodes (dirty and not-dirty) that match
        // option._scope.
        std::vector<std::shared_ptr<GeneratedFileNode>> findGeneratedFiles(
            ExecutionContext* context,
            BuildOptions const& options
        ) const;
    };
}

