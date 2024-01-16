#pragma once

#include "BuildFile.h"

#include <vector>
#include <map>
#include <memory>
#include <filesystem>

namespace YAM {
    class ExecutionContext;
    class Node;
    class GeneratedFileNode;
    class DirectoryNode;
    class CommandNode;
    class GlobNode;
    class GroupNode;

    class __declspec(dllexport) BuildFileCompiler {
    public:
        BuildFileCompiler(
            ExecutionContext* context,
            std::shared_ptr<DirectoryNode> const& baseDir,
            BuildFile::File const& buildFile,
            std::filesystem::path const& globNameSpace = "");

        static std::string compileScript(
            std::filesystem::path const& buildFile,
            BuildFile::Script const& script,
            DirectoryNode const* baseDir,
            std::vector<std::shared_ptr<Node>> cmdInputs,
            std::vector<std::shared_ptr<Node>> orderOnlyInputs,
            std::vector<std::shared_ptr<GeneratedFileNode>> const& outputs);

        // Return whether str contains flag that refers to cmd input,
        // order-only input, output files respectively.
        static bool containsCmdInputFlag(std::string const& str);
        static bool containsOrderOnlyInputFlag(std::string const& str);
        static bool containsOutputFlag(std::string const& str);

        // Return whether the script is literal, i.e. contains no input
        // or output flags.
        static bool isLiteralScript(std::string const& script);

        std::map<std::filesystem::path, std::shared_ptr<CommandNode>>  const& commands() {
            return _commands;
        }

        std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> const& outputs() {
            return _outputs;
        }

        std::map<std::filesystem::path, std::shared_ptr<GroupNode>> const& outputGroups() {
            return _outputGroups;
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

        std::map<std::filesystem::path, std::shared_ptr<GroupNode>> const& newGroups() {
            return _newGroups;
        }

    private:
        std::shared_ptr<GlobNode> compileGlob(
            std::filesystem::path const& pattern);

        std::shared_ptr<GroupNode> compileGroupNode(
            BuildFile::Node const& rule,
            std::filesystem::path const& groupName);

        std::shared_ptr<GroupNode> compileInputGroup(
            BuildFile::Input const& input);

        std::vector<std::shared_ptr<Node>> compileInput(
            BuildFile::Input const& input);

        std::vector<std::shared_ptr<Node>> compileInputs(
            BuildFile::Inputs const& inputs); 
        
        std::vector<std::shared_ptr<Node>> compileOrderOnlyInputs(
            BuildFile::Inputs const& inputs);

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
            std::vector<std::shared_ptr<Node>> const& cmdInputs,
            std::size_t defaultOffset
        ) const;

        std::vector<std::filesystem::path> compileOutputPaths(
            BuildFile::Outputs const& outputs,
            std::vector<std::shared_ptr<Node>> const& cmdInputs
        ) const;

        std::vector<std::filesystem::path> compileIgnoredOutputs(
            BuildFile::Outputs const& outputs
        ) const;

        std::shared_ptr<CommandNode> createCommand(
            BuildFile::Rule const& rule,
            std::vector<std::filesystem::path> const& outputPaths);

        void compileCommand(
            BuildFile::Rule const& rule,
            std::vector<std::shared_ptr<Node>> const& cmdInputs,
            std::vector<std::shared_ptr<Node>> const& orderOnlyInputs);

        void compileOutputGroup(
            BuildFile::Rule const& rule,
            std::vector<std::shared_ptr<GeneratedFileNode>> const& outputs);

        void assertHasNoCmdInputFlag(std::size_t line, std::string const& str) const;
        void assertHasNoOrderOnlyInputFlag(std::size_t line, std::string const& str) const;
        void assertHasNoOutputFlag(std::size_t line, std::string const& str) const;

        void compileRule(BuildFile::Rule const& rule);

    private:
        ExecutionContext* _context;
        std::shared_ptr<DirectoryNode> _baseDir;
        std::filesystem::path _globNameSpace;
        std::filesystem::path _buildFile;

        std::map<std::filesystem::path, std::shared_ptr<CommandNode>> _commands;
        std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> _outputs;
        std::map<std::filesystem::path, std::shared_ptr<GlobNode>> _globs;
        std::map<std::filesystem::path, std::shared_ptr<GroupNode>> _outputGroups;

        std::map<std::filesystem::path, std::shared_ptr<CommandNode>> _newCommands;
        std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> _newOutputs;
        std::map<std::filesystem::path, std::shared_ptr<GlobNode>> _newGlobs;
        std::map<std::filesystem::path, std::shared_ptr<GroupNode>> _newGroups;
    };
}
