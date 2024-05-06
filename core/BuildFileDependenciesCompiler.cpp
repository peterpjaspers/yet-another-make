#include "BuildFileDependenciesCompiler.h"
#include "ExecutionContext.h"
#include "NodeSet.h"
#include "DirectoryNode.h"
#include "SourceFileNode.h"
#include "GlobNode.h"
#include "BuildFileParserNode.h"
#include "BuildFileCompilerNode.h"
#include "Globber.h"
#include "Glob.h"

#include <sstream>
#include <chrono>
#include <ctype.h>

namespace YAM {
    BuildFileDependenciesCompiler::BuildFileDependenciesCompiler(
            ExecutionContext* context,
            std::shared_ptr<DirectoryNode> const& baseDir,
            BuildFile::File const& buildFile,
            Mode compileMode,
            std::filesystem::path const& globNameSpace)
        : _context(context)
        , _baseDir(baseDir)
        , _buildFile(buildFile.buildFile)
        , _globNameSpace(globNameSpace)
    {
        if (compileMode == InputGlobs) {
            for (auto const& glob : buildFile.deps.depGlobs) {
                compileGlob(glob);
            }
            for (auto const& varOrRule : buildFile.variablesAndRules) {
                auto rule = dynamic_cast<BuildFile::Rule*>(varOrRule.get());
                if (rule != nullptr) compileInputs(rule->cmdInputs);
            }
        } else if (compileMode == BuildFileDeps) {
            for (auto const& buildFile : buildFile.deps.depBuildFiles) {
                compileBuildFile(buildFile);
            }
        } else {
            throw std::exception("illegal compileMode");
        }
    }

    std::shared_ptr<GlobNode> BuildFileDependenciesCompiler::findOrCreateGlob(std::filesystem::path const& pattern) {
        auto optimizedBaseDir = _baseDir;
        auto optimizedPattern = pattern;
        Globber::optimize(_context, optimizedBaseDir, optimizedPattern);
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
        return globNode;
    }

    void BuildFileDependenciesCompiler::compileGlob(std::filesystem::path const& pattern) {
        std::shared_ptr<GlobNode> globNode = findOrCreateGlob(pattern);
        _globs.insert({ globNode->name(), globNode });
    }

    std::shared_ptr<SourceFileNode> BuildFileDependenciesCompiler::findBuildFile(
        std::shared_ptr<Node> const& node // directory or source file node
    ) const {
        std::shared_ptr<SourceFileNode> buildFile;
        auto bfDirNode = dynamic_pointer_cast<DirectoryNode>(node);
        if (bfDirNode == nullptr) {
            buildFile = dynamic_pointer_cast<SourceFileNode>(node);
            if (buildFile == nullptr) {
                std::stringstream ss;
                ss << "Buildfile " << _buildFile << " references a non-existing buildfile: " << node->name();
                ss << "." << std::endl;
                throw std::runtime_error(ss.str());
            }
            std::filesystem::path dirPath = buildFile->name().parent_path();
            auto n = _context->nodes().find(dirPath);
            bfDirNode = dynamic_pointer_cast<DirectoryNode>(n);
            if (bfDirNode == nullptr) {
                std::stringstream ss;
                ss << "Buildfile " << _buildFile << " references a non-existing buildfile directory: " << dirPath;
                ss << "." << std::endl;
                throw std::runtime_error(ss.str());
            }      
        } else if (bfDirNode->buildFileParserNode() != nullptr) {
            buildFile = bfDirNode->buildFileParserNode()->buildFile();
            if (buildFile == nullptr) {
                std::stringstream ss;
                ss << "Buildfile " << _buildFile << " references a non-existing buildfile: " << node->name();
                ss << "." << std::endl;
                throw std::runtime_error(ss.str());
            }
        }
        return buildFile;
    }

    void BuildFileDependenciesCompiler::compileBuildFile(std::filesystem::path const& path) {
        if (Glob::isGlob(path.string())) {
            std::shared_ptr<GlobNode> globNode = findOrCreateGlob(path);
            _buildFiles.insert({ globNode->name(), globNode });
        } else {
            auto optimizedBaseDir = _baseDir;
            auto optimizedPath = path;
            Globber::optimize(_context, optimizedBaseDir, optimizedPath);
            std::filesystem::path nodePath(optimizedBaseDir->name());
            if (!optimizedPath.empty()) nodePath = nodePath / optimizedPath;
            std::shared_ptr<Node> const& node = _context->nodes().find(nodePath);
            if (node == nullptr) {
                std::stringstream ss;
                ss << "Buildfile " << _buildFile << " references a non-existing buildfile: " << nodePath;
                ss << "." << std::endl;
                throw std::runtime_error(ss.str());
            }
            std::shared_ptr<SourceFileNode> const& buildFileNode = findBuildFile(node);
            _buildFiles.insert({ buildFileNode->name(), buildFileNode });
        }
    }

    void BuildFileDependenciesCompiler::compileInputs(BuildFile::Inputs const& inputs) {
        std::vector<std::shared_ptr<FileNode>> inputNodes;
        for (auto const& input : inputs.inputs) {
            if (input.pathType == BuildFile::PathType::Glob) {
                compileGlob(input.path);
            }
        }
    }
}
