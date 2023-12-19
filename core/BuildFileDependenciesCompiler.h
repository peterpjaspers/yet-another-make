#pragma once

#include "BuildFile.h"

#include <map>
#include <memory>
#include <filesystem>

namespace YAM {
    class ExecutionContext;
    class DirectoryNode;
    class GlobNode;
    class BuildFileCompilerNode;

    class __declspec(dllexport) BuildFileDependenciesCompiler {
    public:
        BuildFileDependenciesCompiler(
            ExecutionContext* context,
            std::shared_ptr<DirectoryNode> const& baseDir,
            BuildFile::File const& buildFile,
            std::filesystem::path const& globNameSpace = "");

        std::map<std::filesystem::path, std::shared_ptr<BuildFileCompilerNode>> compilers() const {
            return _compilers;
        }
        std::map<std::filesystem::path, std::shared_ptr<GlobNode>> globs() const {
            return _globs;
        }
        std::map<std::filesystem::path, std::shared_ptr<GlobNode>> newGlobs() const {
            return _newGlobs;
        }

    private:
        void compileGlob(std::filesystem::path const& pattern);
        void compileInputs(BuildFile::Inputs const& inputs);
        void compileBuildFile(std::filesystem::path const& buildFileDirectoryPath);

    private:
        ExecutionContext* _context;
        std::shared_ptr<DirectoryNode> _baseDir;
        std::filesystem::path _globNameSpace;

        std::map<std::filesystem::path, std::shared_ptr<BuildFileCompilerNode>> _compilers;
        std::map<std::filesystem::path, std::shared_ptr<GlobNode>> _globs;
        std::map<std::filesystem::path, std::shared_ptr<GlobNode>> _newGlobs;
    };
}
