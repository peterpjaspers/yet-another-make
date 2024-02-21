#pragma once

#include "BuildOptions.h"

#include <vector>
#include <filesystem>

namespace YAM
{
    class ExecutionContext;
    class Node;
    class GeneratedFileNode;

    class __declspec(dllexport) BuildScopeFinder
    {
    public:
        // Return the command nodes that match option._scope.
        // If genFiles != nullptr: file *genFiles with the generated files
        // that match option._scope.
        std::vector<std::shared_ptr<Node>> operator()(
            ExecutionContext* context,
            BuildOptions const& options,
            std::vector<std::shared_ptr<GeneratedFileNode>> *genFiles = nullptr
       ) const;
    };
}

