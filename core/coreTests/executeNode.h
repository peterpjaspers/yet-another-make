#pragma once

#include "..\Node.h"
#include <vector>
#include <memory>

namespace YAMTest
{
    // Execute given node:
    //      - start async execution
    //      - block until async execution completed
    bool executeNode(YAM::Node* n);


    // Execute given nodes:
    //      - start async execution of all nodes (i.e. concurrently execute nodes_
    //      - block until async execution of all nodes completed
    bool executeNodes(std::vector<YAM::Node*> const & nodes);
    bool executeNodes(std::vector<std::shared_ptr<YAM::Node>> const& nodes);
}