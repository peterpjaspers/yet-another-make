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
        std::vector<std::list<GroupNode*>>& cycles
    ) {
        if (!trail.add(group)) {
            auto cycle = trail.trail();
            cycle.insert(cycle.end(), group);
            cycles.push_back(cycle);
            return;
        }
        for (auto const& node : group->group()) {
            auto genFile = dynamic_pointer_cast<GeneratedFileNode>(node);
            if (genFile != nullptr) {
                auto inputGroups = genFile->producer()->inputGroups();
                for (auto const& inputGroup : inputGroups) {
                    findGroupCycle(trail, inputGroup.get(), cycles);
                }
            }
        }
        trail.remove(group);
    }

    void findGroupCycles(
        std::vector<std::shared_ptr<GroupNode>> const& groups
    ) {
        std::vector<std::list<GroupNode*>> cycles;
        for (auto const& group : groups) {
            AcyclicTrail<GroupNode*> trail;
            findGroupCycle(trail, group.get(), cycles);
        }

        if (!cycles.empty()) {
            std::stringstream ss;
            ss << "Cyclic group dependenc";
            (cycles.size() == 1) ? (ss << "y:") : (ss << "ies:");
            ss << std::endl;
            for (auto const& cycle : cycles) {
                ss << trailToString(cycle) << std::endl;
            }
            throw std::runtime_error(ss.str());
        }
    }

    typedef std::list<GroupNode*> Trail;
    typedef std::unordered_set<GroupNode*> TrailSet;
    typedef std::map<Trail const*, TrailSet const*> TrailToTrailSet;
    typedef std::pair<Trail const*, TrailSet const*> TrailPair;

    TrailPair const* prune(TrailPair* pair0, TrailPair* pair1) {
        if (pair0->second->size() > pair1->second->size()) std::swap(pair0, pair1);
        TrailSet const* set0 = pair0->second;
        TrailSet const* set1 = pair1->second;
        for (auto el0 : *set0) {
            if (!set1->contains(el0)) return nullptr;
        }
        // Trails are equivalent, return the shortes one
        if (pair0->first->size() < pair1->first->size()) return pair0;
        return pair1;
    }

    std::vector<Trail> prune(std::vector<Trail> const& cycles) {
        std::vector<TrailSet> sets;
        sets.reserve(cycles.size());
        TrailToTrailSet map;
        for (auto const& trail : cycles) {
            TrailSet set;
            set.insert(trail.begin(), trail.end());
            sets.push_back(std::move(set));
            TrailSet const& setRef = sets[sets.size() - 1];
            TrailSet const* setPtr = &setRef;
            map.insert({ &trail, setPtr });
        }
        std::size_t mapSize;
        do {
            mapSize = map.size();
            TrailToTrailSet prunedMap;
            auto pair = map.begin();
            for (auto it0 = map.begin(); it0 != map.end(); ++it0) {
                TrailPair p0 = *it0;
                TrailPair const* prunedPair = nullptr;
                auto it1 = it0;
                it1++;
                for (; it1 != map.end(); ++it1) {
                    TrailPair p1 = *it1;
                    TrailPair const* candidate = prune(&p0, &p1);
                    if (candidate != nullptr) {
                        if (prunedPair == nullptr) prunedPair = candidate;
                        else if (candidate->first->size() < prunedPair->first->size()) {
                            prunedPair = candidate;
                        }
                    }
                }
                if (prunedPair != nullptr) {
                    prunedMap.insert(*prunedPair);
                } else {
                    prunedMap.insert(*it0);
                }
            }
            map = prunedMap;
        } while (map.size() < mapSize);

        std::vector<Trail> prunedTrails;
        for (auto const& pair : map) {
            prunedTrails.push_back(*(pair.first));
        }
        return prunedTrails;
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
        for (auto const& group : groups) {
            AcyclicTrail<GroupNode*> trail;
            findGroupCycle(trail, group.get(), _cycles);
        }
    }

    std::vector<std::list<GroupNode*>> GroupCycleFinder::uniqueCycles() const {
        return prune(_cycles);
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

    std::string GroupCycleFinder::cyclesToString(std::vector<std::list<GroupNode*>> const& cycles) const {
        if (cycles.empty()) return "";

        std::stringstream ss;
        ss << "Cyclic group dependenc";
        (cycles.size() == 1) ? (ss << "y:") : (ss << "ies:");
        ss << std::endl;
        for (auto const& cycle : cycles) {
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