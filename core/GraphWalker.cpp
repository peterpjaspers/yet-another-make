#include "GraphWalker.h"
#include "Node.h"

namespace
{
    using namespace YAM;

}
namespace YAM
{
    void GraphWalker::walk(
        std::vector<Node*> const& roots,
        std::unordered_set<Node*>& visited,
        Delegate<void, Node*, std::unordered_set<Node*>&> visit
    ) {
        for (auto r : roots) {
            visit.Execute(r, visited);
        }
    }

    GraphWalker::GraphWalker(Node* root, GraphType type, Delegate<bool, Node*> include)
        : GraphWalker(std::vector<Node*>{ root }, type, include)
    {}

    GraphWalker::GraphWalker(std::vector<Node*> const& roots, GraphType type, Delegate<bool, Node*> include)
        : _type(type)
        , _include(include)
    {
        auto visit = Delegate<void, Node*, std::unordered_set<Node*>&>::CreateRaw(this, &GraphWalker::_visit);
        walk(roots, _visited, visit);
    }

    void GraphWalker::_visit(Node* node, std::unordered_set<Node*>& visited) {
        if (!visited.insert(node).second) return; // node was already visited
        if (_include.Execute(node)) _included.push_back(node);
        std::vector<Node*> nodes;
        switch (_type) {
        case Prerequisites: {
            std::vector<std::shared_ptr<Node>> _nodes;
            if (node->supportsPrerequisites()) node->getPrerequisites(_nodes);
            for (auto const& n : _nodes) nodes.push_back(n.get());
            break;
        }
        case Postrequisites: {
            std::vector<std::shared_ptr<Node>> _nodes;
            if (node->supportsPostrequisites()) node->getPostrequisites(_nodes);
            for (auto const& n : _nodes) nodes.push_back(n.get());
            break;
        }
        case Preparents: {
            for (auto n : node->preParents()) nodes.push_back(n);
            break;
        }
        case Postparents: {
            for (auto n : node->postParents()) nodes.push_back(n);
            break;
        }
        default: throw std::exception("unknown walker type");
        }
        for (auto n : nodes) _visit(n, visited);
    }

    std::unordered_set<Node*> const& GraphWalker::visited() {
        return _visited;
    }

    void GraphWalker::appendVisited(std::vector<Node*>& visited) const {
        for (auto n : _visited) visited.push_back(n);
    }

    std::vector<Node*> const& GraphWalker::included() {
        return _included;
    }
}


