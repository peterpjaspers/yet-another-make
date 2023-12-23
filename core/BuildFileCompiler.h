#pragma once

#include "BuildFile.h"

#include <vector>
#include <map>
#include <memory>
#include <filesystem>

namespace YAM {
    class ExecutionContext;
    class FileNode;
    class GeneratedFileNode;
    class DirectoryNode;
    class CommandNode;
    class GlobNode;

    class __declspec(dllexport) BuildFileCompiler {
    public:
        BuildFileCompiler(
            ExecutionContext* context,
            std::shared_ptr<DirectoryNode> const& baseDir,
            BuildFile::File const& buildFile,
            std::filesystem::path const& globNameSpace = "");

        std::map<std::filesystem::path, std::shared_ptr<CommandNode>>  const& commands() {
            return _commands;
        }

        std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> const& outputs() {
            return _outputs;
        }

        // Return the globs that were used to resolve rule inputs.
        std::map<std::filesystem::path, std::shared_ptr<GlobNode>> const& globs() {
            return _globs;
        }

        std::map<std::filesystem::path, std::shared_ptr<CommandNode>> const& newCommands() const {
            return _newCommands;
        }

        std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> const& newOutputs() const {
            return _newOutputs;
        }

        std::map<std::filesystem::path, std::shared_ptr<GlobNode>> const& newGlobs() const {
            return _newGlobs;
        }

    private:
        std::shared_ptr<CommandNode> createCommand(
            std::vector<std::filesystem::path> const& outputPaths);

        void addCommand(
            BuildFile::Rule const& rule,
            std::vector<std::shared_ptr<FileNode>> const& cmdInputs,
            std::vector<std::shared_ptr<GeneratedFileNode>> const& orderOnlyInputs);

        std::shared_ptr<GlobNode> compileGlob(
            std::filesystem::path const& pattern);

        std::vector<std::shared_ptr<FileNode>> compileInput(
            BuildFile::Input const& input);

        std::vector<std::shared_ptr<FileNode>> compileInputs(
            BuildFile::Inputs const& inputs);

        static std::string compileScript(
            std::filesystem::path const& buildFile,
            BuildFile::Script const& script,
            DirectoryNode const* baseDir,
            std::vector<std::shared_ptr<FileNode>> cmdInputs,
            std::vector<std::shared_ptr<GeneratedFileNode>> orderOnlyInputs,
            std::vector<std::shared_ptr<GeneratedFileNode>> outputs);

        std::shared_ptr<GeneratedFileNode> createGeneratedFileNode(
            BuildFile::Rule const& rule,
            std::shared_ptr<CommandNode> const& cmdNode,
            std::filesystem::path const& outputPath);

        std::vector<std::shared_ptr<GeneratedFileNode>> createGeneratedFileNodes(
            BuildFile::Rule const& rule,
            std::shared_ptr<CommandNode> const& cmdNode,
            std::vector<std::filesystem::path> const& outputPaths);

        std::filesystem::path compileOutputPath(
            BuildFile::Output const& output,
            std::vector<std::shared_ptr<FileNode>> const& cmdInputs,
            std::size_t defaultOffset
        ) const;

        std::vector<std::filesystem::path> compileOutputPaths(
            BuildFile::Outputs const& outputs,
            std::vector<std::shared_ptr<FileNode>> const& cmdInputs
        ) const;

        std::vector<std::filesystem::path> compileIgnoredOutputs(
            BuildFile::Outputs const& outputs
        ) const;

        void compileRule(BuildFile::Rule const& rule);

    private:
        ExecutionContext* _context;
        std::shared_ptr<DirectoryNode> _baseDir;
        std::filesystem::path _globNameSpace;
        std::filesystem::path _buildFile;

        std::map<std::filesystem::path, std::shared_ptr<CommandNode>> _commands;
        std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> _outputs;
        std::map<std::filesystem::path, std::shared_ptr<GlobNode>> _globs;

        std::map<std::filesystem::path, std::shared_ptr<CommandNode>> _newCommands;
        std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> _newOutputs;
        std::map<std::filesystem::path, std::shared_ptr<GlobNode>> _newGlobs;
    };
}
