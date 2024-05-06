#pragma once
#include <vector>
#include <string>
#include <memory>
#include <list>

namespace YAM
{
    class BuildFileCompilerNode;
    class GroupNode;

    class __declspec(dllexport) GroupCycleFinder
    {
    public:
        GroupCycleFinder(std::vector<std::shared_ptr<BuildFileCompilerNode>> const& compilers);
        GroupCycleFinder(std::vector<std::shared_ptr<GroupNode>> const& groups);

        // Return the cyclic trails in the group node dependency graph.
        std::vector<std::list<GroupNode*>> const& cycles() { return _cycles; }

        // Return the set of groups involved in cycles()
        std::vector<GroupNode*> cyclingGroups() const;

        std::string cyclesToString() const;
        std::string cyclingGroupsToString() const;

    private:
        std::vector<std::list<GroupNode*>> _cycles;
    };
}

