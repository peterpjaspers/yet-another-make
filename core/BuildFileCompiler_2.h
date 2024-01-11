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
    class GroupNode;
    class Command;

    class __declspec(dllexport) BuildFileCompiler_2 {
    public:
        BuildFileCompiler_2(
            ExecutionContext* context,
            std::shared_ptr<DirectoryNode> const& baseDir,
            BuildFile::File const& buildFile,
            std::filesystem::path const& globNameSpace = "");

        std::map<std::filesystem::path, std::shared_ptr<CommandNode>>  const& commands() {
            return _commandNodes;
        }

        std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> const& outputs() {
            return _outputNodes;
        }

        std::map<std::filesystem::path, std::shared_ptr<GroupNode>> const& outputGroups() {
            return _outputGroupNodes;
        }

        std::map<std::filesystem::path, std::shared_ptr<GroupNode>> const& inputGroups() {
            return _inputGroupNodes;
        }

        // Return the globs that were used to resolve rule inputs.
        std::map<std::filesystem::path, std::shared_ptr<GlobNode>> const& globs() {
            return _globNodes;
        }

        std::map<std::filesystem::path, std::shared_ptr<CommandNode>> const& newCommands() const {
            return _newCommandNodes;
        }

        std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> const& newOutputs() const {
            return _newOutputNodes;
        }

        std::map<std::filesystem::path, std::shared_ptr<GlobNode>> const& newGlobs() const {
            return _newGlobNodes;
        }

        std::map<std::filesystem::path, std::shared_ptr<GroupNode>> const& newInputGroups() {
            return _newInputGroupNodes;
        }

        std::map<std::filesystem::path, std::shared_ptr<GroupNode>> const& newOutputGroups() {
            return _newOutputGroupNodes;
        }

    private:
        std::shared_ptr<GlobNode> compileGlob(
            std::filesystem::path const& pattern);

        std::vector<std::shared_ptr<FileNode>> compileInputGroup(
            BuildFile::Input const& input);

        std::vector<std::shared_ptr<FileNode>> compileInput(
            BuildFile::Input const& input);

        std::vector<std::shared_ptr<FileNode>> compileInputs(
            BuildFile::Inputs const& inputs);

        std::vector<std::shared_ptr<GeneratedFileNode>> compileOrderOnlyInputs(
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

        std::shared_ptr<CommandNode> createCommand(
            std::vector<std::filesystem::path> const& outputPaths);

        void compileCommand(
            BuildFile::Rule const& rule,
            std::vector<std::shared_ptr<FileNode>> const& cmdInputs,
            std::vector<std::shared_ptr<GeneratedFileNode>> const& orderOnlyInputs);

        void compileOutputGroup(
            BuildFile::Rule const& rule,
            std::vector<std::shared_ptr<GeneratedFileNode>> const& outputs);

        void assertScriptHasNoCmdInputFlag(BuildFile::Rule const& rule) const;
        void assertScriptHasNoOrderOnlyInputFlag(BuildFile::Rule const& rule) const;
        void assertScriptHasNoOutputFlag(BuildFile::Rule const& rule) const;

        void compileRule(BuildFile::Rule const& rule);

    private:
        ExecutionContext* _context;
        std::shared_ptr<DirectoryNode> _baseDir;
        std::filesystem::path _globNameSpace;
        std::filesystem::path _buildFile;

        std::map<std::filesystem::path, std::shared_ptr<Command>> _commands;

        std::map<std::filesystem::path, std::shared_ptr<CommandNode>> _commandNodes;
        std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> _outputNodes;
        std::map<std::filesystem::path, std::shared_ptr<GlobNode>> _globNodes;
        std::map<std::filesystem::path, std::shared_ptr<GroupNode>> _inputGroupNodes;
        std::map<std::filesystem::path, std::shared_ptr<GroupNode>> _outputGroupNodes;

        std::map<std::filesystem::path, std::shared_ptr<CommandNode>> _newCommandNodes;
        std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> _newOutputNodes;
        std::map<std::filesystem::path, std::shared_ptr<GlobNode>> _newGlobNodes;
        std::map<std::filesystem::path, std::shared_ptr<GroupNode>> _newInputGroupNodes;
        std::map<std::filesystem::path, std::shared_ptr<GroupNode>> _newOutputGroupNodes;
    };
}
