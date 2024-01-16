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

        // Return all cycling trails in the group node dependency graph.
        // Note: these trails typically contain duplicates. E.g.
        // g1 => g2 => g1 and g2 => g1 => g2
        std::vector<std::list<GroupNode*>> const& cycles() { return _cycles; }

        // Return the unique trails in cycles()/
        // Note;: O(N), where N is cycles().size() time complexity.
        std::vector<std::list<GroupNode*>> uniqueCycles() const;

        // Return the set of groups involved in cycles()
        std::vector<GroupNode*> cyclingGroups() const;

        std::string cyclesToString(std::vector<std::list<GroupNode*>> const& cycles) const;
        std::string cyclingGroupsToString() const;

    private:
        std::vector<std::list<GroupNode*>> _cycles;
    };
}

