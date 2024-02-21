#include "BuildScopeFinder.h"
#include "Glob.h"
#include "Globber.h"
#include "ExecutionContext.h"
#include "DirectoryNode.h"
#include "GeneratedFileNode.h"
#include "CommandNode.h"
#include "FileRepository.h"

namespace
{
    using namespace YAM;

    void scopeGlobs(
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
            Globber::optimize(baseDir, pattern);
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
        std::shared_ptr<CommandNode> const& cmd
    ) {
        for (auto const& output : cmd->outputs()) {
            for (auto const& glob : scope) {
                if (glob.matches(output->name())) return true;
            }
        }
        return false;
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
}

namespace YAM
{
    std::vector<std::shared_ptr<Node>> BuildScopeFinder::operator()(
        ExecutionContext* context,
        BuildOptions const& options,
        std::vector<std::shared_ptr<GeneratedFileNode>> *genFiles
    ) const {
        std::vector<std::shared_ptr<Node>> scope;
        if (!options._scope.empty()) {
            std::vector<std::filesystem::path> paths;
            std::vector<Glob> globs;
            scopeGlobs(context, options, paths, globs);
            for (auto const& path : paths) {
                auto node = context->nodes().find(path);
                auto output = dynamic_pointer_cast<GeneratedFileNode>(node);
                if (output != nullptr && output->name() == path) {
                    auto const& cmd = output->producer();
                    if (cmd->state() == Node::State::Dirty) {
                        scope.push_back(output->producer());
                    }
                    if (genFiles != nullptr) genFiles->push_back(output);
                }
            } 
            for (auto const& node : context->nodes().nodes()) {
                auto cmd = dynamic_pointer_cast<CommandNode>(node);
                if (cmd != nullptr && genFiles != nullptr) {
                    for (auto const& genFile : cmd->outputs()) {
                        if (inScope(globs, genFile)) genFiles->push_back(genFile);
                    }
                }
                if (
                    cmd != nullptr
                    && cmd->state() == Node::State::Dirty
                    && inScope(globs, cmd)
                 ) {
                    scope.push_back(cmd);
                }
            }
        } else {
            for (auto const& node : context->nodes().nodes()) {
                auto cmd = dynamic_pointer_cast<CommandNode>(node);
                if (cmd != nullptr && cmd->state() == Node::State::Dirty) {
                    scope.push_back(cmd);
                    if (genFiles != nullptr) {
                        auto const &outputs = cmd->outputs();
                        genFiles->insert(genFiles->end(), outputs.begin(), outputs.end());
                    }
                }
            }
        }
        return scope;
    }
}