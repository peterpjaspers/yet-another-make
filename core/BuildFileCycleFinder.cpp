#include "BuildFileCycleFinder.h"
#include "BuildFileParserNode.h"
#include "SourceFileNode.h"
#include "AcyclicTrail.h"

namespace
{
    using namespace YAM;

    std::string trailToString(std::list<BuildFileParserNode *> const& trail) {
        std::stringstream ss;
        auto end = std::prev(trail.end());
        auto it = trail.begin();
        for (; it != end; ++it) {
            ss << (*it)->buildFile()->name() << " => ";
        }
        ss << (*it)->name() << std::endl;
        return ss.str();
    }

    void findCycle(
        AcyclicTrail<BuildFileParserNode *> &trail,
        BuildFileParserNode *parser,
        std::vector<std::list<BuildFileParserNode *>> &cycles,
        std::unordered_set<BuildFileParserNode *> &doneParsers
    ) {
        if (doneParsers.contains(parser)) return;
        if (!trail.add(parser)) {
            auto cycle = trail.trail();
            cycle.insert(cycle.end(), parser);
            cycles.push_back(cycle);
            return;
        }
        for (auto inputParser : parser->dependencies()) {
            findCycle(trail, const_cast<BuildFileParserNode*>(inputParser), cycles, doneParsers);
        }
        trail.remove(parser);
        doneParsers.insert(parser);
    }
}

namespace YAM
{

    BuildFileCycleFinder::BuildFileCycleFinder(
        std::vector<std::shared_ptr<BuildFileParserNode>> const& parsers
    ) {
        std::unordered_set<BuildFileParserNode*> doneParsers;
        for (auto const& parser : parsers) {
            AcyclicTrail<BuildFileParserNode*> trail;
            findCycle(trail, parser.get(), _cycles, doneParsers);
        }
    }

    // Return the set of groups involved in cycles()
    std::vector<BuildFileParserNode*> BuildFileCycleFinder::cyclingParsers() const {
        std::unordered_set<BuildFileParserNode*> parserSet;
        for (auto const& trail : _cycles) {
            parserSet.insert(trail.begin(), trail.end());
        }
        std::vector<BuildFileParserNode*> parsers(parserSet.begin(), parserSet.end());
        struct CompareName {
            constexpr bool operator()(const BuildFileParserNode* lhs, const BuildFileParserNode* rhs) const {
                return lhs->name() < rhs->name();
            }
        };
        CompareName cmp;
        std::sort(parsers.begin(), parsers.end(), cmp);
        return parsers;
    }

    std::string BuildFileCycleFinder::cyclesToString() const {
        if (_cycles.empty()) return "";

        std::stringstream ss;
        ss << "Cyclic buildfile dependenc";
        (_cycles.size() == 1) ? (ss << "y:") : (ss << "ies:");
        ss << std::endl;
        for (auto const& cycle : _cycles) {
            ss << trailToString(cycle) << std::endl;
        }
        return ss.str();
    }

    std::string BuildFileCycleFinder::cyclingBuildFilesToString() const {
        auto parsers = cyclingParsers();
        if (parsers.empty()) return "";

        std::stringstream ss;
        ss << "Circular dependencies found among the following buildfiles:" << std::endl;
        for (auto parser : parsers) ss << parser->buildFile()->name() << std::endl;
        return ss.str();
    }
}