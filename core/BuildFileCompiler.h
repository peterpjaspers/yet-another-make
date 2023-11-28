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
        std::vector<std::shared_ptr<FileNode>> compileInput(
            BuildFile::Input const& input,
            std::vector<std::shared_ptr<FileNode>> included);

        std::vector<std::shared_ptr<FileNode>> compileInputs(
            BuildFile::Inputs const& inputs);

        std::string compileScript(
            BuildFile::Script const& script,
            std::vector<std::shared_ptr<FileNode>> cmdInputs,
            std::vector<std::shared_ptr<GeneratedFileNode>> orderOnlyInputs,
            std::vector<std::shared_ptr<GeneratedFileNode>> outputs);

        std::vector<std::shared_ptr<GeneratedFileNode>> compileOutput(
            BuildFile::Output const& output,
            std::vector<std::shared_ptr<FileNode>> const& inputs);

        std::vector<std::shared_ptr<GeneratedFileNode>> compileOutputs(
            BuildFile::Outputs const& outputs,
            std::vector<std::shared_ptr<FileNode>> const& inputs);

        void compileRule(BuildFile::Rule const& rule);

        void addCommand(
            BuildFile::Rule const& rule,
            std::vector<std::shared_ptr<FileNode>> const& cmdInputs,
            std::vector<std::shared_ptr<GeneratedFileNode>> const& orderOnlyInputs);

        ExecutionContext* _context;
        std::shared_ptr<DirectoryNode> _baseDir;
        std::vector<std::shared_ptr<CommandNode>> _commands;
    };
}
