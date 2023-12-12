#pragma once

#include "BuildFile.h"

#include <map>
#include <memory>
#include <filesystem>

namespace YAM {
    class ExecutionContext;
    class GeneratedFileNode;
    class DirectoryNode;
    class GlobNode;
    class BuildFileProcessingNode;

    class __declspec(dllexport) BuildFileDependenciesCompiler {
    public:
        BuildFileDependenciesCompiler(
            ExecutionContext* context,
            std::shared_ptr<DirectoryNode> const& baseDir,
            BuildFile::File const& buildFile,
            std::filesystem::path const& globNameSpace = "");

        std::map<std::filesystem::path, std::shared_ptr<BuildFileProcessingNode>> processingNodes() const {
            return _processingNodes;
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
        void compileBFPN(std::filesystem::path const& buildFileDirectoryPath);

    private:
        ExecutionContext* _context;
        std::shared_ptr<DirectoryNode> _baseDir;
        std::filesystem::path _globNameSpace;

        std::map<std::filesystem::path, std::shared_ptr<BuildFileProcessingNode>> _processingNodes;
        std::map<std::filesystem::path, std::shared_ptr<GlobNode>> _globs;
        std::map<std::filesystem::path, std::shared_ptr<GlobNode>> _newGlobs;
    };
}
