#include "BuildScopeFinder.h"
#include "Glob.h"
#include "Globber.h"
#include "ExecutionContext.h"
#include "DirectoryNode.h"
#include "GeneratedFileNode.h"
#include "CommandNode.h"
#include "FileRepositoryNode.h"

namespace
{
    using namespace YAM;

    static const std::string cmdClass("CommandNode");

    template<typename Tin, typename Tout>
    void castNodes(
        std::vector<std::shared_ptr<Tin>> const& nodes,
        std::vector<std::shared_ptr<Tout>>& tNodes
    ) {
        for (auto const& node : nodes) {
            auto tnode = dynamic_pointer_cast<Tout>(node);
            if (tnode == nullptr) throw std::runtime_error("not a node of type T");
            tNodes.push_back(tnode);
        }
    }

    void appendDirtyNodes(
        ExecutionContext* context,
        std::string const& nodeClass,
        std::vector<std::shared_ptr<Node>>& dirtyNodes
    ) {
        auto const& dirtyNodesMap = context->nodes().dirtyNodes();
        auto it = dirtyNodesMap.find(nodeClass);
        if (it != dirtyNodesMap.end()) {
            for (auto const& node : it->second) {
                if (node->state() != Node::State::Dirty) throw std::runtime_error("not a dirty node");
                auto repoType = node->repository()->repoType();
                if (repoType == FileRepositoryNode::RepoType::Build) {
                    dirtyNodes.push_back(node);
                }
            }
        }
    }

    void appendAllDirtyNodes(
        ExecutionContext* context,
        std::vector<std::shared_ptr<Node>>& dirtyNodes
    ) {
        auto const& dirtyNodesMap = context->nodes().dirtyNodes();
        for (auto const& clsPair : dirtyNodesMap) {
            for (auto const& node : clsPair.second) {
                if (node->state() != Node::State::Dirty) throw std::runtime_error("not a dirty node");
                auto repoType = node->repository()->repoType();
                if (repoType == FileRepositoryNode::RepoType::Build) {
                    dirtyNodes.push_back(node);
                }
            }
        }
    }

    void appendDirtyCommands(
        ExecutionContext* context,
        std::vector<std::shared_ptr<CommandNode>>& dirtyCmds
    ) {
        std::vector<std::shared_ptr<Node>> dirtyNodes;
        appendDirtyNodes(context, cmdClass, dirtyNodes);
        castNodes<Node, CommandNode>(dirtyNodes, dirtyCmds);
    }

    void getScopeGlobsAndPaths(
        ExecutionContext* context,
        BuildOptions const& options,
        std::vector<std::filesystem::path>& paths,
        std::vector<Glob>& globs
    ) {
        auto repo = context->findRepositoryContaining(options._workingDir);
        if (repo == nullptr) throw std::runtime_error("Not in a known repository: " + options._workingDir.string());
        auto symWdPath = repo->symbolicPathOf(options._workingDir);
        auto wdNode = dynamic_pointer_cast<DirectoryNode>(context->nodes().find(symWdPath));
        if (wdNode == nullptr) throw std::runtime_error("No such directory node: " + symWdPath.string());
        for (auto const& path : options._scope) {
            auto baseDir = wdNode;
            auto pattern = path;
            Globber::optimize(wdNode->context(), baseDir, pattern);
            auto optPattern = baseDir->name() / pattern;
            if (Glob::isGlob(optPattern.string())) {
                Glob glob(optPattern);
                globs.push_back(glob);
            } else {
                paths.push_back(optPattern);
            }
        }
    }

    bool inScope(
        std::vector<Glob> const& scope,
        std::shared_ptr<GeneratedFileNode> const &genFile
    ) {
        for (auto const& glob : scope) {
            if (glob.matches(genFile->name())) return true;
        }
        return false;
    }

    bool inScope(
        std::vector<Glob> const& scope,
        std::shared_ptr<CommandNode> const& cmd
    ) {
        for (auto const& genFile : cmd->outputs()) {
            if (inScope(scope, genFile)) {
                return true;
            }
        }
        return false;
    }

    void findDirtyCmdsByPaths(
        ExecutionContext* context, 
        std::vector<std::filesystem::path> const &paths,
        std::vector<std::shared_ptr<CommandNode>>& scope
    ) {
        if (!paths.empty()) {
            std::unordered_set<std::shared_ptr<CommandNode>> cmdSet;
            auto const& nodes = context->nodes();
            for (auto const& path : paths) {
                auto const& genFile = dynamic_pointer_cast<GeneratedFileNode>(nodes.find(path));
                if (genFile != nullptr) {
                    auto const& cmd = genFile->producer();
                    cmdSet.insert(cmd);
                }
            }
            scope.insert(scope.end(), cmdSet.begin(), cmdSet.end());
        }
    }

    void findDirtyCmdsByGlobs(
        ExecutionContext* context,
        std::vector<Glob> const &globs,
        std::vector<std::shared_ptr<CommandNode>>& scope
    ) {
        if (!globs.empty()) {
            std::vector<std::shared_ptr<CommandNode>> dirtyCmds;
            appendDirtyCommands(context, dirtyCmds);
            std::unordered_set<std::shared_ptr<CommandNode>> cmdSet;
            for (auto const& cmd : dirtyCmds) {
                if (inScope(globs, cmd)) scope.push_back(cmd);
            }
        }
    }

    void findGenFilesAll(
        ExecutionContext* context, 
        std::vector<std::shared_ptr<GeneratedFileNode>> &genFiles
    ) {
        for (auto const& node : context->nodes().nodes()) {
            auto const& genFile = dynamic_pointer_cast<GeneratedFileNode>(node);
            if (genFile != nullptr) genFiles.push_back(genFile);            
        }
    }

    void findGenFilesByGlobs(
        ExecutionContext* context,
        std::vector<Glob> const& globs,
        std::vector<std::shared_ptr<GeneratedFileNode>>& genFiles
    ) {
        if (!globs.empty()) {
            for (auto const& node : context->nodes().nodes()) {
                auto const& genFile = dynamic_pointer_cast<GeneratedFileNode>(node);
                if (genFile != nullptr) {
                    if (inScope(globs, genFile)) genFiles.push_back(genFile);
                }
            }
        }
    }

    void findGenFilesByPaths(
        ExecutionContext* context,
        std::vector<std::filesystem::path> const& paths,
        std::vector<std::shared_ptr<GeneratedFileNode>> &genFiles
    ) {
        if (!paths.empty()) {
            auto const& nodes = context->nodes();
            for (auto const& path : paths) {
                auto const& genFile = dynamic_pointer_cast<GeneratedFileNode>(nodes.find(path));
                if (genFile != nullptr) genFiles.push_back(genFile);
            }
        }
    }
}

namespace YAM
{
    std::vector<std::shared_ptr<CommandNode>> BuildScopeFinder::findDirtyCommands(
        ExecutionContext* context,
        BuildOptions const& options
    ) const {
        std::vector<std::shared_ptr<CommandNode>> cmdScope;
        if (options._scope.empty()) {
            appendDirtyCommands(context, cmdScope);
        } else {
            std::vector<std::filesystem::path> paths;
            std::vector<Glob> globs;
            getScopeGlobsAndPaths(context, options, paths, globs);
            findDirtyCmdsByPaths(context, paths, cmdScope);
            findDirtyCmdsByGlobs(context, globs, cmdScope);
        }
        return cmdScope;
    }

    std::vector<std::shared_ptr<GeneratedFileNode>> BuildScopeFinder::findGeneratedFiles(
        ExecutionContext* context,
        BuildOptions const& options
    ) const {
        std::vector<std::shared_ptr<GeneratedFileNode>> genFileScope;
        if (options._scope.empty()) {
            findGenFilesAll(context, genFileScope);
        } else {
            std::vector<std::filesystem::path> paths;
            std::vector<Glob> globs;
            getScopeGlobsAndPaths(context, options, paths, globs);
            findGenFilesByPaths(context, paths, genFileScope);
            findGenFilesByGlobs(context, globs, genFileScope);
        }
        return genFileScope;
    }
}