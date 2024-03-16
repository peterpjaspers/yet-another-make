#include "GroupCycleFinder.h"
#include "BuildFileCompilerNode.h"
#include "GroupNode.h"
#include "GeneratedFileNode.h"
#include "CommandNode.h"
#include "AcyclicTrail.h"

namespace
{
    using namespace YAM;

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

    std::string trailToString(std::list<GroupNode*> const& trail) {
        std::stringstream ss;
        auto end = std::prev(trail.end());
        auto it = trail.begin();
        for (; it != end; ++it) {
            ss << (*it)->name() << " => ";
        }
        ss << (*it)->name() << std::endl;
        return ss.str();
    }

    void findGroupCycle(
        AcyclicTrail<GroupNode*>& trail,
        GroupNode* group,
        std::vector<std::list<GroupNode*>>& cycles,
        std::unordered_set<GroupNode*>& doneGroups
    ) {
        if (doneGroups.contains(group)) return;
        if (!trail.add(group)) {
            auto cycle = trail.trail();
            cycle.insert(cycle.end(), group);
            cycles.push_back(cycle);
            return;
        }
        for (auto const &node : group->content()) {
            auto const &genFile = dynamic_pointer_cast<GeneratedFileNode>(node);
            std::shared_ptr<CommandNode> command; 
            if (genFile != nullptr) {
                command = genFile->producer();
            } else {
                command = dynamic_pointer_cast<CommandNode>(node);
            }
            if (command != nullptr) {
                auto inputGroups = command->inputGroups();
                for (auto const& inputGroup : inputGroups) {
                    findGroupCycle(trail, inputGroup.get(), cycles, doneGroups);
                }
            }
        }
        trail.remove(group);
        doneGroups.insert(group);
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
        std::unordered_set<GroupNode*> doneGroups;
        for (auto const& group : groups) {
            AcyclicTrail<GroupNode*> trail;
            findGroupCycle(trail, group.get(), _cycles, doneGroups);
        }
    }

    // Return the set of groups involved in cycles()
    std::vector<GroupNode*> GroupCycleFinder::cyclingGroups() const {
        std::unordered_set<GroupNode*> groupSet;
        for (auto const& trail : _cycles) {
            groupSet.insert(trail.begin(), trail.end());
        }
        std::vector<GroupNode*> groups(groupSet.begin(), groupSet.end());
        struct CompareName {
            constexpr bool operator()(const GroupNode* lhs, const GroupNode* rhs) const {
                return lhs->name() < rhs->name();
            }
        };
        CompareName cmp;
        std::sort(groups.begin(), groups.end(), cmp);
        return groups;
    }

    std::string GroupCycleFinder::cyclesToString() const {
        if (_cycles.empty()) return "";

        std::stringstream ss;
        ss << "Cyclic group dependenc";
        (_cycles.size() == 1) ? (ss << "y:") : (ss << "ies:");
        ss << std::endl;
        for (auto const& cycle : _cycles) {
            ss << trailToString(cycle) << std::endl;
        }
        return ss.str();
    }

    std::string GroupCycleFinder::cyclingGroupsToString() const {
        auto groups = cyclingGroups();
        if (groups.empty()) return "";

        std::stringstream ss;
        ss << "Circular dependencies found among the following groups:" << std::endl;
        for (auto group : groups) ss << group->name() << std::endl;
        return ss.str();
    }

}