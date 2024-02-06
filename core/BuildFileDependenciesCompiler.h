#pragma once

#include "BuildFile.h"

#include <map>
#include <memory>
#include <filesystem>

namespace YAM {
    class ExecutionContext;
    class Node;
    class DirectoryNode;
    class GlobNode;
    class SourceFileNode;

    class __declspec(dllexport) BuildFileDependenciesCompiler {
    public:
        enum Mode { 
            // compile globs from glob dependency section and from cmd and
            // order-only input sections
            InputGlobs,
            // compile globs from buildfile dependency section
            BuildFileDeps,
            // compile all globs
            Both
        };
        BuildFileDependenciesCompiler(
            ExecutionContext* context,
            std::shared_ptr<DirectoryNode> const& baseDir,
            BuildFile::File const& buildFile,
            Mode compileMode,
            std::filesystem::path const& globNameSpace = "");

        // The DirectoryNodes and/or SourceFileNodes and/or GlobNodes that define
        // the buildfile paths and/or globs specified in the buildFile dependency 
        // section in the buildFile parseTree.
        std::map<std::filesystem::path, std::shared_ptr<Node>> buildFiles() const {
            return _buildFiles;
        }

        // The globs used in cmd and order-only input sections of the rules
        // in buildFile.
        std::map<std::filesystem::path, std::shared_ptr<GlobNode>> globs() const {
            return _globs;
        }

    private:
        std::shared_ptr<SourceFileNode> findBuildFile(std::shared_ptr<Node> const& node) const;
        std::shared_ptr<GlobNode> findOrCreateGlob(std::filesystem::path const& pattern);
        void compileGlob(std::filesystem::path const& pattern);
        void compileInputs(BuildFile::Inputs const& inputs);
        void compileBuildFile(std::filesystem::path const& path);

    private:
        ExecutionContext* _context;
        std::shared_ptr<DirectoryNode> _baseDir;
        std::filesystem::path _globNameSpace;

        std::map<std::filesystem::path, std::shared_ptr<Node>> _buildFiles;
        std::map<std::filesystem::path, std::shared_ptr<GlobNode>> _globs;

        // _newGlobs contains newly created globs, i.e. globs that were not 
        // present in context.
        std::map<std::filesystem::path, std::shared_ptr<GlobNode>> _newGlobs;
    };
}
