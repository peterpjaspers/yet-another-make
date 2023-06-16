#pragma once

#include "Delegates.h"

#include <vector>
#include <unordered_set>

namespace YAM
{
    class Node;

    class __declspec(dllexport) GraphWalker
    {
    public:
        enum GraphType
        {
            Prerequisites,
            Postrequisites,
            Dependants,
            Postparents
        };

        // roots.each(r => visit(r, visited)
        // The 'visit' delegate chooses whether to include r in 'visited'
        // and whether to follow references from r to other nodes.
        // The visit delegate must not follow nodes that are already
        // in 'visited'.
        static void walk(
            std::vector<Node*> const& roots,
            std::unordered_set<Node*>& visited,
            Delegate<void, Node*, std::unordered_set<Node*>&> visit);

        // Construct a walker that walks 'roots' according 'type' and that
        // and includes visited nodes when include.Execute(node) == true. 
        GraphWalker(
            Node* roots,
            GraphType type,
            Delegate<bool, Node*> include = Delegate<bool, Node*>::CreateLambda([](Node* n) { return true; }));
        GraphWalker(
            std::vector<Node*> const& roots, 
            GraphType type, 
            Delegate<bool, Node*> include = Delegate<bool, Node*>::CreateLambda([](Node* n) { return true; }));

        // Get all nodes that were reachable according to walker type.
        std::unordered_set<Node*> const& visited();
        void appendVisited(std::vector<Node*>& visited) const;

        // Return the subset of visited nodes for whcih the include delegate 
        // returns true.
        std::vector<Node*> const& included();

    private:
        void _visit(Node* node, std::unordered_set<Node*>& visited);

        std::vector<Node*> _roots;
        GraphType _type;
        Delegate<bool, Node*> _include;
        std::unordered_set<Node*> _visited;
        std::vector<Node*> _included;
    };
}

