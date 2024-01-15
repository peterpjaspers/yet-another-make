#pragma once
#include <vector>
#include <string>
#include <memory>

namespace YAM
{
    class BuildFileCompilerNode;
    class GroupNode;

    class __declspec(dllexport) GroupCycleFinder
    {
    public:
        GroupCycleFinder(std::vector<std::shared_ptr<BuildFileCompilerNode>> const& compilers);
        GroupCycleFinder(std::vector<std::shared_ptr<GroupNode>> const& groups);

        std::vector<std::string> const& cycles() { return _cycles; }

    private:
        std::vector<std::string> _cycles;
    };
}

