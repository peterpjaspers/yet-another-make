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
    class FileNode;
    class CommandNode;
    class GlobNode;
    class GroupNode;

    class __declspec(dllexport) BuildFileCompiler {
    public:
        BuildFileCompiler(
            ExecutionContext* context,
            std::shared_ptr<DirectoryNode> const& baseDir,
            BuildFile::File const& buildFile,
            // Results of previous compilation of this file
            std::map<std::filesystem::path, std::shared_ptr<CommandNode>> const& commands,
            std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> const &outputs,
            std::map<std::filesystem::path, std::shared_ptr<GroupNode>> outputGroups,
            std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> const& allowedInputs,
            // Optional, to allow globs to be named in a private workspace as  
            // to limit their re-usability across buildfiles.
            std::filesystem::path const& globNameSpace = "");

        std::map<std::filesystem::path, std::shared_ptr<CommandNode>> const& commands() const {
            return _commands;
        }

        std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> const& outputs() const {
            return _outputs;
        }

        std::map<std::filesystem::path, std::shared_ptr<GroupNode>> const& outputGroups() const {
            return _outputGroups;
        }

        // Return the globs that were used to resolve rule inputs.
        std::map<std::filesystem::path, std::shared_ptr<GlobNode>> const& globs() const {
            return _globs;
        }

        std::map<std::shared_ptr<CommandNode>, std::size_t> ruleLineNrs() const {
            return _ruleLineNrs;
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
        void updateOutputGroups();

        std::shared_ptr<GlobNode> compileGlob(
            std::filesystem::path const& pattern);

        std::shared_ptr<GroupNode> compileGroupNode(
            std::filesystem::path const& groupName);

        std::vector<std::shared_ptr<GeneratedFileNode>>& compileBin(
            std::filesystem::path const& binPath);

        std::vector<std::shared_ptr<GeneratedFileNode>>& compileInputBin(
            BuildFile::Input const& input);

        std::shared_ptr<GroupNode> compileInputGroup(
            BuildFile::Input const& input);

        std::shared_ptr<FileNode> compileInputPath(
            BuildFile::Input const& input
        );

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

        std::shared_ptr<GroupNode> compileOutputGroup(
            std::filesystem::path const& groupPath);

        void compileOutputBin(
            std::filesystem::path const& binPath,
            std::vector<std::shared_ptr<GeneratedFileNode>> const& outputs);

        void compileOutputGroupContent(
            std::filesystem::path const& groupName,
            std::vector<std::shared_ptr<GeneratedFileNode>> const& outputs);

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

        void compileBin(
            BuildFile::Rule const& rule,
            std::filesystem::path const& binPath,
            std::vector<std::shared_ptr<GeneratedFileNode>> const& outputs);

        void compileRule(BuildFile::Rule const& rule);

    private:
        ExecutionContext* _context;
        std::shared_ptr<DirectoryNode> _baseDir;
        std::filesystem::path _globNameSpace;
        std::filesystem::path _buildFile;

        // Results of previous compilations.
        std::map<std::filesystem::path, std::shared_ptr<CommandNode>> _oldCommands;
        std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> _oldOutputs;
        std::map<std::filesystem::path, std::shared_ptr<GroupNode>> _oldOutputGroups;
        std::map<std::filesystem::path, std::vector<std::shared_ptr<Node>>> _oldOutputGroupsContent;
        std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> _allowedInputs;
        
        // Results of this compilation. The globs are the globs found in cmd and
        // order-only input of rules.
        std::map<std::filesystem::path, std::shared_ptr<CommandNode>> _commands;
        std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> _outputs;
        std::map<std::filesystem::path, std::shared_ptr<GlobNode>> _globs;
        std::map<std::filesystem::path, std::shared_ptr<GroupNode>> _outputGroups;
        // For each group in _oldOutputGroups the subset of _oldOutputs in that group.

        std::map<std::filesystem::path, std::vector<std::shared_ptr<GeneratedFileNode>>> _bins;
        // Used to collect the new contributions to output groups. Once all 
        // commands have been created _outputGroups is updated with this info.
        // Rationale: only update groups when needed.
        std::map<std::filesystem::path, std::vector<std::shared_ptr<Node>>> _outputGroupsContent;

        // The line nrs of the rules from which the commands were compiled.
        // Ordering is as in _commands.
        std::map<std::shared_ptr<CommandNode>, std::size_t> _ruleLineNrs;

        // Newly created objects because existing ones were not found in the 
        // execution context. It is up-to the user of this class to add them to
        // the context.
        std::map<std::filesystem::path, std::shared_ptr<CommandNode>> _newCommands;
        std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> _newOutputs;
        std::map<std::filesystem::path, std::shared_ptr<GlobNode>> _newGlobs;
        std::map<std::filesystem::path, std::shared_ptr<GroupNode>> _newGroups;
    };
}
