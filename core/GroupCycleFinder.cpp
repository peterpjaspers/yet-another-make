#include "GroupCycleFinder.h"
#include "BuildFileCompilerNode.h"
#include "GroupNode.h"
#include "GeneratedFileNode.h"
#include "CommandNode.h"
#include "AcyclicTrail.h"

namespace
{
    using namespace YAM;

    std::string groupCycleMessage(
        GroupNode* closingGroup,
        std::list< GroupNode*> const& trail
    ) {
        std::stringstream ss;
        for (auto grp : trail) ss << grp->name() << " => ";
        //ss << closingGroup->name() << std::endl;
        ss << std::endl;
        return ss.str();
    }

    // Find whether group is part of a group cycle.
    // Post: if (returned group == nullptr) then no cycle
    // Else adding returned group to trail would make trail cyclic.
    GroupNode* findGroupCycle(AcyclicTrail<GroupNode*>& trail, GroupNode* group) {
        if (!trail.add(group)) {
            return group;
        }
        for (auto const& node : group->group()) {
            auto genFile = dynamic_pointer_cast<GeneratedFileNode>(node);
            if (genFile != nullptr) {
                for (auto const& inputGroup : genFile->producer()->inputGroups()) {
                    auto closingGroup = findGroupCycle(trail, inputGroup.get());
                    if (closingGroup != nullptr) return closingGroup;
                }
            }
        }
        trail.remove(group);
        return nullptr;
    }

    void findGroupCycle(
        AcyclicTrail<GroupNode*>& trail,
        GroupNode* group,
        std::unordered_set<GroupNode*>& visited,
        std::vector<std::string>& cycles
    ) {
        bool notVisited = visited.insert(group).second;
        //if (notVisited) {
        if (true) {
            auto closingGroup = findGroupCycle(trail, group);
            if (closingGroup != nullptr) {
                cycles.push_back(groupCycleMessage(closingGroup, trail.trail()));
            }
        }
    }

    void findGroupCycles(
        std::map<std::filesystem::path, std::shared_ptr<GroupNode>> const& groups,
        std::vector<std::string>& cycles
    ) {
        std::unordered_set<GroupNode*> visited;
        AcyclicTrail<GroupNode*> trail;
        for (auto const& pair : groups) {
            findGroupCycle(trail, pair.second.get(), visited, cycles);
        }
    }

    std::vector<std::shared_ptr<GroupNode>> outputGroupsOf(
        std::vector<std::shared_ptr<BuildFileCompilerNode>> const& compilers
    ) {
        std::vector<std::shared_ptr<GroupNode>> groups;
        for (auto const& compiler : compilers) {
            for (auto const& pair : compiler->outputGroups()) {
                groups.push_back(pair.second);
            }
        }
        return groups;
    }
 }

namespace YAM
{
    GroupCycleFinder::GroupCycleFinder(
        std::vector<std::shared_ptr<BuildFileCompilerNode>> const& compilers
    ) : GroupCycleFinder(outputGroupsOf(compilers))
    {}

    GroupCycleFinder::GroupCycleFinder(
        std::vector<std::shared_ptr<GroupNode>> const& groups
    ) {
        std::unordered_set<GroupNode*> visited;
        for (auto const& group : groups) {
            AcyclicTrail<GroupNode*> trail;
            findGroupCycle(trail, group.get(), visited, _cycles);
        }
    }
}