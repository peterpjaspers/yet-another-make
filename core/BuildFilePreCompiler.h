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

    class __declspec(dllexport) BuildFilePreCompiler {
    public:
        BuildFilePreCompiler(
            ExecutionContext* context,
            std::shared_ptr<DirectoryNode> const& baseDir,
            BuildFile::File const& buildFile,
            std::filesystem::path const& globNameSpace = "");
                
        struct Command {
            // paths reference source file, generated file or group
            std::vector<std::filesystem::path> _cmdInputs;
            // paths reference generated file or group
            std::vector<std::filesystem::path> _orderOnlyInputs;
            // paths reference generated files
            std::vector<std::filesystem::path> _outputs;
            std::filesystem::path _outputGroup;
        };

        std::vector<std::shared_ptr<Command>> const& commands();

    private:
        void findCompilers();
        void findGlobs();

        std::vector<std::filesystem::path> compileGlob(std::filesystem::path const& pattern);
        std::vector<std::filesystem::path> compileInputGroup(BuildFile::Input const& input);
        std::vector<std::filesystem::path> compileInput(BuildFile::Input const& input);
        std::vector<std::filesystem::path> compileInputs(BuildFile::Inputs const& inputs);
        std::vector<std::filesystem::path> compileOrderOnlyInputs(BuildFile::Inputs const& inputs);

        static std::string compileScript(
            std::filesystem::path const& buildFile,
            BuildFile::Script const& script,
            DirectoryNode const* baseDir,
            std::vector<std::filesystem::path> cmdInputs,
            std::vector<std::filesystem::path> orderOnlyInputs,
            std::vector<std::filesystem::path> outputs);

        std::filesystem::path compileOutputPath(
            BuildFile::Output const& output,
            std::vector<std::filesystem::path> const& cmdInputs,
            std::size_t defaultOffset
        ) const;

        std::vector<std::filesystem::path> compileOutputPaths(
            BuildFile::Outputs const& outputs,
            std::vector<std::filesystem::path> const& cmdInputs
        ) const;

        std::vector<std::filesystem::path> compileIgnoredOutputs(
            BuildFile::Outputs const& outputs
        ) const;

        std::shared_ptr<Command> createCommand(
            std::vector<std::filesystem::path> const& outputPaths);

        void compileCommand(
            BuildFile::Rule const& rule,
            std::vector<std::filesystem::path> const& cmdInputs,
            std::vector<std::filesystem::path> const& orderOnlyInputs);

        void compileOutputGroup(
            BuildFile::Rule const& rule,
            std::vector<std::filesystem::path> const& outputs);

        void assertScriptHasNoCmdInputFlag(BuildFile::Rule const& rule) const;
        void assertScriptHasNoOrderOnlyInputFlag(BuildFile::Rule const& rule) const;
        void assertScriptHasNoOutputFlag(BuildFile::Rule const& rule) const;

        void compileRule(BuildFile::Rule const& rule);

        void validate();

    private:
        ExecutionContext* _context;
        std::shared_ptr<DirectoryNode> _baseDir;
        std::filesystem::path _globNameSpace; 
        BuildFile::File const& _buildFile;

        // Build file path => compiler node that compiles that buildfile
        std::map<std::filesystem::path, std::shared_ptr<BuildFileCompilerNode>> _inputCompilers;
        // Glob path pattern => glob node that evaluates the glob 
        std::map<std::filesystem::path, std::shared_ptr<GlobNode>> _inputGlobs;

        // output file path => Command that produces that file
        std::map<std::filesystem::path, std::shared_ptr<Command>> _commands;
    };
}
