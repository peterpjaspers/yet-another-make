#pragma once

#include "Glob.h"
#include "BuildFile.h"

#include <vector>
#include <memory>
#include <filesystem>
#include <regex>

namespace YAM {
    class ExecutionContext;
    class Node;
    class FileNode;
    class GeneratedFileNode;
    class DirectoryNode;
    class CommandNode;

    class __declspec(dllexport) BuildFileCompiler {
    public:
        BuildFileCompiler(
            ExecutionContext* context,
            std::shared_ptr<DirectoryNode> const& baseDir,
            BuildFile::File const& buildFile);

        std::vector<std::shared_ptr<CommandNode>> const& commands() {
            return _commands;
        }

    private:
        std::vector<std::filesystem::path> expandOutputPaths(
            BuildFile::Outputs const& outputs,
            std::vector<std::shared_ptr<FileNode>> const& cmdInputs
        ) const;

        std::shared_ptr<CommandNode> createCommand(
            std::vector<std::filesystem::path> const& outputPaths) const;

        void addCommand(
            BuildFile::Rule const& rule,
            std::shared_ptr<CommandNode> const& cmdNode,
            std::vector<std::shared_ptr<FileNode>> const& cmdInputs,
            std::vector<std::shared_ptr<GeneratedFileNode>> const& orderOnlyInputs,
            std::vector<std::filesystem::path> const& outputPaths);

        std::vector<std::shared_ptr<FileNode>> compileInput(
            BuildFile::Input const& input,
            std::vector<std::shared_ptr<FileNode>> included) const;

        std::vector<std::shared_ptr<FileNode>> compileInputs(
            BuildFile::Inputs const& inputs) const;

        std::string compileScript(
            BuildFile::Script const& script,
            std::vector<std::shared_ptr<FileNode>> cmdInputs,
            std::vector<std::shared_ptr<GeneratedFileNode>> orderOnlyInputs,
            std::vector<std::shared_ptr<GeneratedFileNode>> outputs) const;

        std::vector<std::shared_ptr<GeneratedFileNode>> compileOutput(
            BuildFile::Output const& output,
            std::vector<std::shared_ptr<FileNode>> const& inputs) const;

        std::vector<std::shared_ptr<GeneratedFileNode>> compileOutputs(
            std::shared_ptr<CommandNode> const& cmdNode,
            std::vector<std::filesystem::path> const& outputPaths,
            std::vector<std::shared_ptr<FileNode>> const& inputs) const;

        void compileRule(BuildFile::Rule const& rule);

        ExecutionContext* _context;
        std::shared_ptr<DirectoryNode> _baseDir;
        std::vector<std::shared_ptr<CommandNode>> _commands;
    };
}
