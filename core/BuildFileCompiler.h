#pragma once

#include "BuildFile.h"
#include "Node.h"
#include "CommandNode.h"

#include <vector>
#include <set>
#include <map>
#include <memory>
#include <filesystem>

namespace YAM {
    class ExecutionContext;
    class GeneratedFileNode;
    class DirectoryNode;
    class FileNode;
    class GlobNode;
    class GroupNode;

    // Compiles the rules in the parse tree of a buildfile into command, 
    // mandatory output, group and input glob nodes. Input globs are the globs
    // found in the input sections of the rules. The mandatory output is the 
    // union of all mandatory outputs of the commands.
    // 
    // Takes as input the results of the previous compilation of the buildfile. 
    // Returns the command, group and glob nodes. Results from the previous
    // compilation are re-used when possible.
    // New nodes are not added to the execution and nodes that could not be
    // re-used are not removed from the execution context. Adding/removing node
    // to the context is responsibility of caller.
    // One exception: if a rule has an input group that does not exist, then a
    // new GroupNode is created and added to the context.
    // 
    // In case of compilation errors: a std::runtime_exception is thrown with
    // a message that explains the error. Re-used nodes may have been changed.
    // Newly added input groups are not removed from the context.
    //
    class __declspec(dllexport) BuildFileCompiler {
    public:

        BuildFileCompiler(
            ExecutionContext* context,
            std::shared_ptr<DirectoryNode> const& baseDir,
            BuildFile::File const& buildFile,
            // Results of previous compilation
            std::map<std::filesystem::path, std::shared_ptr<CommandNode>> const &commands,
            std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> const &mandatoryOutputs,
            std::map<std::filesystem::path, std::shared_ptr<GroupNode>> const &outputGroups,
            // Generated input files that are allowed to be used as inputs by the
            // rules in buildFile.
            std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> const& allowedInputs,
            // Optional, to allow globs to be named in a private workspace as  
            // to limit their re-usability across buildfiles.
            std::filesystem::path const& globNameSpace = "");

        std::map<std::filesystem::path, std::shared_ptr<CommandNode>> const& commands() const {
            return _commands;
        }
        std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> const& mandatoryOutputs() const {
            return _mandatoryOutputs;
        }
        std::map<std::filesystem::path, std::shared_ptr<GroupNode>> const& outputGroups() const {
            return _outputGroups;
        }
        std::map<std::filesystem::path, std::shared_ptr<GlobNode>> const& inputGlobs() const {
            return _inputGlobs;
        }

        // Return for each command the line number of the rule from which it is 
        // derived.
        std::map<std::shared_ptr<CommandNode>, std::size_t> ruleLineNrs() const {
            return _ruleLineNrs;
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
            CommandNode::OutputNodes const& outputs);

        void compileOutputGroupContent(
            std::filesystem::path const& groupName,
            std::shared_ptr<CommandNode> const& cmdNode);

        std::filesystem::path compileOutputPath(
            BuildFile::Output const& output,
            std::vector<std::shared_ptr<Node>> const& cmdInputs,
            std::size_t defaultInputOffset
        ) const;

        std::vector<std::filesystem::path> compileMandatoryOutputPaths(
            BuildFile::Outputs const& outputs,
            std::vector<std::shared_ptr<Node>> const& cmdInputs
        ) const;

        CommandNode::OutputFilter compileOutputFilter(
            BuildFile::Output const& output,
            std::vector<std::shared_ptr<Node>> const& cmdInputs,
            std::size_t defaultOffset
        ) const;

        std::vector<CommandNode::OutputFilter> compileOutputFilters(
            BuildFile::Outputs const& outputs,
            std::vector<std::shared_ptr<Node>> const& cmdInputs
        ) const;

        std::vector<std::filesystem::path> compileIgnoredOutputs(
            BuildFile::Outputs const& outputs
        ) const;

        std::shared_ptr<CommandNode> createCommand(
            BuildFile::Rule const& rule,
            std::filesystem::path const& firstOutputPath);

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
        std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> _oldMandatoryOutputs;
        std::map<std::filesystem::path, std::shared_ptr<GroupNode>> _oldOutputGroups;

        // Per group the contribution to that group from _oldCommands
        std::map<std::filesystem::path, std::set<std::shared_ptr<Node>, Node::CompareName>> _oldOutputGroupsContent;
        std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> _allowedInputs;
        
        // Results of this compilation. The globs are the globs found in cmd input
        // and order-only input of rules.
        std::map<std::filesystem::path, std::shared_ptr<CommandNode>> _commands; 
        std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> _mandatoryOutputs;
        std::map<std::filesystem::path, std::shared_ptr<GroupNode>> _outputGroups;
        std::map<std::filesystem::path, std::shared_ptr<GlobNode>> _inputGlobs;

        // Per group the contribution to that group from _commands
        // Once all compilation is done _outputGroups is updated from 
        // _oldOutputGroupsContent and _outputGroupsContent.
        // Rationale: only update groups when needed.
        std::map<std::filesystem::path, std::set<std::shared_ptr<Node>, Node::CompareName>> _outputGroupsContent;

        // Per bin the content of that bin
        std::map<std::filesystem::path, std::vector<std::shared_ptr<GeneratedFileNode>>> _bins;

        // The line nrs of the rules from which the commands were compiled.
        // Ordering is as in _commands.
        std::map<std::shared_ptr<CommandNode>, std::size_t> _ruleLineNrs;

        // Newly created nodes because existing ones were not found in the 
        // execution context. 
        std::map<std::filesystem::path, std::shared_ptr<CommandNode>> _newCommands;
        std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> _newMandatoryOutputs;
        std::map<std::filesystem::path, std::shared_ptr<GroupNode>> _newOutputGroups;
        std::map<std::filesystem::path, std::shared_ptr<GlobNode>> _newInputGlobs;
    };
}
