#include "BuildScopeFinder.h"
#include "Glob.h"
#include "Globber.h"
#include "ExecutionContext.h"
#include "DirectoryNode.h"
#include "GeneratedFileNode.h"
#include "CommandNode.h"
#include "ForEachNode.h"
#include "FileRepositoryNode.h"

namespace
{
    using namespace YAM;

    static const std::string cmdClass("CommandNode");
    static const std::string forEachClass("ForEachNode");
    static const std::string genFileClass("GeneratedFileNode");

    bool pathMatchesGlobs(
        std::vector<Glob> const& globs,
        std::filesystem::path const& path
    ) {
        for (auto const& glob : globs) {
            if (glob.matches(path)) return true;
        }
        return false;
    }

    bool pathMatchesPaths(
        std::vector<std::filesystem::path> const& paths,
        std::filesystem::path const& path
    ) {
        for (auto const& p : paths) {
            if (p == path) return true;
        }
        return false;
    }

    bool cmdMatchesGlobs(
        std::vector<Glob> const& globs,
        std::shared_ptr<CommandNode> const& cmd
    ) {
        if (globs.empty()) return true;
        for (auto const& pair : cmd->mandatoryOutputs()) {
            if (pathMatchesGlobs(globs, pair.second->name())) {
                return true;
            }
        }
        return false;
    }

    bool cmdMatchesPaths(
        std::vector<std::filesystem::path> const& paths,
        std::shared_ptr<CommandNode> const& cmd
    ) {
        if (paths.empty()) return true;
        for (auto const& pair : cmd->mandatoryOutputs()) {
            if (pathMatchesPaths(paths, pair.second->name())) {
                return true;
            }
        }
        return false;
    }

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

    void findDirtyCmdsByPaths(
        ExecutionContext* context, 
        std::vector<std::filesystem::path> const &paths,
        std::vector<std::shared_ptr<Node>>& scope
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
        std::vector<std::shared_ptr<Node>>& scope
    ) {
        if (!globs.empty()) {
            std::vector<std::shared_ptr<Node>> dirtyNodes;
            appendDirtyNodes(context, cmdClass, dirtyNodes);
            std::vector<std::shared_ptr<CommandNode>> dirtyCmds;
            castNodes<Node, CommandNode>(dirtyNodes, dirtyCmds);
            for (auto const& cmd : dirtyCmds) {
                if (cmdMatchesGlobs(globs, cmd)) scope.push_back(cmd);
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
            std::vector<std::shared_ptr<GeneratedFileNode>> allGenFiles;
            findGenFilesAll(context, allGenFiles);
            for (auto const& genFile : allGenFiles) {
                if (pathMatchesGlobs(globs, genFile->name())) genFiles.push_back(genFile);
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
}

namespace YAM
{
    BuildScopeFinder::BuildScopeFinder(ExecutionContext* context, BuildOptions const& options)
        : _context(context)
    {
        if (!options._scope.empty()) {
            std::vector<Glob> globs;
            getScopeGlobsAndPaths(context, options, _paths, _globs);
        }
    }

    std::vector<std::shared_ptr<Node>> BuildScopeFinder::dirtyCommands() const {
        std::vector<std::shared_ptr<Node>> dirtyCmds;
        if (_paths.empty() && _globs.empty()) {
            appendDirtyNodes(_context, cmdClass, dirtyCmds);
        } else {
            findDirtyCmdsByPaths(_context, _paths, dirtyCmds);
            findDirtyCmdsByGlobs(_context, _globs, dirtyCmds);
        }
        std::vector<std::shared_ptr<Node>> dirtyNodes;
        appendDirtyNodes(_context, forEachClass, dirtyNodes);
        if (!dirtyNodes.empty()) {
            std::vector<std::shared_ptr<ForEachNode>> dirtyForEach;
            castNodes<Node, ForEachNode>(dirtyNodes, dirtyForEach);
            std::unordered_set<std::shared_ptr<Node>> feCommands;
            for (auto const& fe : dirtyForEach) {
                for (auto const& cmd : fe->commands()) feCommands.insert(cmd);
            }
            for (auto const& cmd : dirtyCmds) {
                if (!feCommands.contains(cmd)) dirtyNodes.push_back(cmd);
            }
            return dirtyNodes;
        } else {
            return dirtyCmds;
        }
    }

    std::vector<std::shared_ptr<GeneratedFileNode>> BuildScopeFinder::generatedFiles() const {
        std::vector<std::shared_ptr<GeneratedFileNode>> genFileScope;
        if (_paths.empty() && _globs.empty()) {
            findGenFilesAll(_context, genFileScope);
        } else {
            findGenFilesByPaths(_context, _paths, genFileScope);
            findGenFilesByGlobs(_context, _globs, genFileScope);
        }
        return genFileScope;
    }

    bool BuildScopeFinder::inScope(std::shared_ptr<CommandNode> const& cmd) const {
        if (cmd->state() != Node::State::Dirty) return false;
        if (cmdMatchesPaths(_paths, cmd)) return true;
        if (cmdMatchesGlobs(_globs, cmd)) return true;
        return false;
    }

}