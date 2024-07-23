#pragma once

#include "GlobNode.h"
#include "DirectoryNode.h"
#include "GeneratedFileNode.h"
#include "CommandNode.h"
#include "ForEachNode.h"
#include "GroupNode.h"
#include "BuildFileCompilerNode.h"

#include <map>
#include <filesystem>

namespace
{
    using namespace YAM;

    void addNode(std::shared_ptr<BuildFileCompilerNode> compiler, StateObserver* observer) {
        // owned and added to context by DirectoryNode
        compiler->addObserver(observer);
    }
    void addNode(std::shared_ptr<GlobNode> glob, StateObserver* observer) {
        // A glob node can be shared by multiple compilers.
        glob->context()->nodes().addIfAbsent(glob);
        glob->addObserver(observer);
    }
    void addNode(std::shared_ptr<CommandNode> node, StateObserver* observer) {
        node->context()->nodes().add(node);
    }
    void addNode(std::shared_ptr<ForEachNode> node, StateObserver* observer) {
        node->context()->nodes().add(node);
    }
    void addNode(std::shared_ptr<GeneratedFileNode> node, StateObserver* observer) {
        node->context()->nodes().add(node);
        DirectoryNode::addGeneratedFile(node);
    }
    void addNode(std::shared_ptr<GroupNode> group, StateObserver* observer) {
        // A group node can be shared by multiple compilers and can already
        // have been added to the context by another compiler.
        group->context()->nodes().addIfAbsent(group);
    }
    void addNode(std::shared_ptr<Node> node, StateObserver* observer) {
        auto glob = dynamic_pointer_cast<GlobNode>(node);
        if (glob != nullptr) addNode(glob, observer);
        else node->addObserver(observer);
    }

    void removeNode(std::shared_ptr<BuildFileCompilerNode> compiler, StateObserver* observer) {
        // owned by DirectoryNode
        compiler->removeObserver(observer);
    }
    void removeNode(std::shared_ptr<GlobNode> glob, StateObserver* observer) {
        // A glob node can be shared by multiple compilers.
        glob->removeObserver(observer);
        if (glob->observers().empty()) {
            glob->context()->nodes().remove(glob);
        }
    }
    void removeNode(std::shared_ptr<CommandNode> cmd, StateObserver* observer) {
        cmd->context()->nodes().remove(cmd);
    }
    void removeNode(std::shared_ptr<ForEachNode> node, StateObserver* observer) {
        node->context()->nodes().remove(node);
    }
    void removeNode(std::shared_ptr<GeneratedFileNode> node, StateObserver* observer) {
        DirectoryNode::removeGeneratedFile(node);
        node->context()->nodes().remove(node);
    }
    void removeNode(std::shared_ptr<GroupNode> group, StateObserver* observer) {
        if (group->content().empty() && group->observers().empty()) {
            group->context()->nodes().remove(group);
        }
    }
    void removeNode(std::shared_ptr<Node> node, StateObserver* observer) {
        auto glob = dynamic_pointer_cast<GlobNode>(node);
        if (glob != nullptr) removeNode(glob, observer);
        else node->removeObserver(observer);
    }
}

namespace YAM
{
    template<class TNode>
    void computeMapsDifference(
        std::map<std::filesystem::path, std::shared_ptr<TNode>> const& in1,
        std::map<std::filesystem::path, std::shared_ptr<TNode>> const& in2,
        std::map<std::filesystem::path, std::shared_ptr<TNode>>& inBoth,
        std::map<std::filesystem::path, std::shared_ptr<TNode>>& onlyIn1,
        std::map<std::filesystem::path, std::shared_ptr<TNode>>& onlyIn2
    ) {
        for (auto i1 : in1) {
            if (in2.find(i1.first) != in2.end()) inBoth.insert(i1);
            else onlyIn1.insert(i1);
        }
        for (auto i2 : in2) {
            if (in1.find(i2.first) == in1.end()) onlyIn2.insert(i2);
        }
    }

    template <class TNode>
    void updateMap(
        ExecutionContext* context,
        StateObserver* observer,
        std::map<std::filesystem::path, std::shared_ptr<TNode>>& toUpdate,
        std::map<std::filesystem::path, std::shared_ptr<TNode>> const& newSet
    ) {
        std::map<std::filesystem::path, std::shared_ptr<TNode>> kept;
        std::map<std::filesystem::path, std::shared_ptr<TNode>> added;
        std::map<std::filesystem::path, std::shared_ptr<TNode>> removed;
        computeMapsDifference(newSet, toUpdate, kept, added, removed);
        for (auto const& pair : added) addNode(pair.second, observer);
        for (auto const& pair : removed) removeNode(pair.second, observer);
        toUpdate = newSet;
    }
}
