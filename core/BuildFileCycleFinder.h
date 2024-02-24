#pragma once
#include <vector>
#include <string>
#include <memory>
#include <list>

namespace YAM
{
    class BuildFileParserNode;

    class __declspec(dllexport) BuildFileCycleFinder
    {
    public:
        BuildFileCycleFinder(std::vector<std::shared_ptr<BuildFileParserNode>> const& parsers);

        // Return the cyclic trails in the group node dependency graph.
        std::vector<std::list<BuildFileParserNode*>> const& cycles() { return _cycles; }

        // Return the set of parsers involved in cycles()
        std::vector<BuildFileParserNode*> cyclingParsers() const;

        std::string cyclesToString() const;
        std::string cyclingBuildFilesToString() const;

    private:
        std::vector<std::list<BuildFileParserNode*>> _cycles;
    };
}

