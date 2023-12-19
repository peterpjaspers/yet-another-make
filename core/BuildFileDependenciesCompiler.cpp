#include "BuildFileDependenciesCompiler.h"
#include "ExecutionContext.h"
#include "NodeSet.h"
#include "DirectoryNode.h"
#include "BuildFileCompilerNode.h"
#include "GlobNode.h"
#include "Globber.h"
#include "Glob.h"

#include <sstream>
#include <chrono>
#include <ctype.h>

namespace
{
    using namespace YAM;

}

namespace YAM {
    BuildFileDependenciesCompiler::BuildFileDependenciesCompiler(
            ExecutionContext* context,
            std::shared_ptr<DirectoryNode> const& baseDir,
            BuildFile::File const& buildFile,
            std::filesystem::path const& globNameSpace)
        : _context(context)
        , _baseDir(baseDir)
        , _globNameSpace(globNameSpace)
    {
        for (auto const& glob : buildFile.deps.depGlobs) {
            compileGlob(glob);
        }
        for (auto const& buildFile : buildFile.deps.depBuildFiles) {
            compileBuildFile(buildFile);
        }
        for (auto const& varOrRule : buildFile.variablesAndRules) {
            auto rule = dynamic_cast<BuildFile::Rule*>(varOrRule.get());
            if (rule != nullptr) compileInputs(rule->cmdInputs);
        }
    }

    void BuildFileDependenciesCompiler::compileGlob(std::filesystem::path const& pattern) {
        auto optimizedBaseDir = _baseDir;
        auto optimizedPattern = pattern;
        Globber::optimize(optimizedBaseDir, optimizedPattern);
        std::filesystem::path globName(_globNameSpace / optimizedBaseDir->name() / optimizedPattern);
        auto globNode = dynamic_pointer_cast<GlobNode>(_context->nodes().find(globName));
        if (globNode == nullptr) {
            auto it = _newGlobs.find(globName);
            if (it != _newGlobs.end()) globNode = it->second;
        }
        if (globNode == nullptr) {
            globNode = std::make_shared<GlobNode>(_context, globName);
            globNode->baseDirectory(optimizedBaseDir);
            globNode->pattern(optimizedPattern);
            _newGlobs.insert({ globNode->name(), globNode });
        }
        _globs.insert({ globNode->name(), globNode });
    }

    void BuildFileDependenciesCompiler::compileBuildFile(std::filesystem::path const& buildFileDirectoryPath) {
        auto optimizedBaseDir = _baseDir;
        auto optimizedPath = buildFileDirectoryPath;
        Globber::optimize(optimizedBaseDir, optimizedPath);
        std::filesystem::path bfDirName(optimizedBaseDir->name());
        auto bfDirNode = dynamic_pointer_cast<DirectoryNode>(_context->nodes().find(bfDirName));
        if (bfDirNode == nullptr) {
            throw std::runtime_error("No such directory: " + bfDirName.string());
        }
        std::shared_ptr<BuildFileCompilerNode> bfcn = bfDirNode->buildFileCompilerNode();
        if (bfcn == nullptr) {
            throw std::runtime_error("No buildfile found in directory " + bfDirName.string());
        }
        _compilers.insert({ bfcn->name(), bfcn });
    }

    void BuildFileDependenciesCompiler::compileInputs(BuildFile::Inputs const& inputs) {
        std::vector<std::shared_ptr<FileNode>> inputNodes;
        for (auto const& input : inputs.inputs) {
            if (Glob::isGlob(input.pathPattern.string())) {
                compileGlob(input.pathPattern);
            }
        }
    }
}
