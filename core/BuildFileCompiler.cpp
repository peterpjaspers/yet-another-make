#include "BuildFileCompiler.h"
#include "ExecutionContext.h"
#include "FileNode.h"
#include "SourceFileNode.h"
#include "GeneratedFileNode.h"
#include "DirectoryNode.h"
#include "CommandNode.h"

#include <sstream>

namespace
{
    using namespace YAM;

    std::filesystem::path uidPath() {
        std::stringstream ss;
        ss << rand();
        return std::filesystem::path(ss.str());
    }

    std::filesystem::path uniqueCmdName(NodeSet& nodes) {
        std::filesystem::path uid = uidPath();
        std::shared_ptr<Node> node = nodes.find(uid);
        while (node != nullptr) {
            uid = uidPath();
            node = nodes.find(uid);
        }
        return uid;
    }
}

namespace YAM {
    BuildFileCompiler::BuildFileCompiler(
            ExecutionContext* context,
            std::shared_ptr<DirectoryNode> const& baseDir,
            BuildFile::File const& buildFile)
        : _context(context)
        , _baseDir(baseDir)
    {
        for (auto const& varOrRule : buildFile.variablesAndRules) {
            auto rule = dynamic_cast<BuildFile::Rule*>(varOrRule.get());
            if (rule != nullptr) compileRule(*rule);
        }
    }

    std::vector<std::filesystem::path> BuildFileCompiler::expandOutputPaths(
        BuildFile::Outputs const& outputs,
        std::vector<std::shared_ptr<FileNode>> const& cmdInputs
    ) const {
        std::vector<std::filesystem::path> outputPaths;
        // TODO: resolve %B etc
        for (auto const& output : outputs.outputs) {
            std::filesystem::path outputPath = _baseDir->name() / output.path;
            outputPaths.push_back(outputPath);
        }
        return outputPaths;

    }

    std::shared_ptr<CommandNode> BuildFileCompiler::createCommand(
        std::vector<std::filesystem::path> const& outputPaths) const
    {
        std::shared_ptr<CommandNode> cmdNode;
        std::filesystem::path cmdName;
        if (outputPaths.empty()) {
            cmdName = uniqueCmdName(_context->nodes());
        } else {
            cmdName = outputPaths[0] / "__cmd";
        }
        std::shared_ptr<Node> node = _context->nodes().find(cmdName);
        cmdNode = dynamic_pointer_cast<CommandNode>(node);
        if (cmdNode == nullptr) {
            cmdNode = std::make_shared<CommandNode>(_context, cmdName);
        }
        return cmdNode;
    }

    void BuildFileCompiler::addCommand(
        BuildFile::Rule const& rule,
        std::shared_ptr<CommandNode> const& cmdNode,
        std::vector<std::shared_ptr<FileNode>> const& cmdInputs,
        std::vector<std::shared_ptr<GeneratedFileNode>> const& orderOnlyInputs,
        std::vector<std::filesystem::path> const& outputPaths
    ) {
        std::vector<std::shared_ptr<GeneratedFileNode>> outputs = compileOutputs(cmdNode, outputPaths, cmdInputs);
        std::string script = compileScript(rule.script, cmdInputs, orderOnlyInputs, outputs);
        cmdNode->script(script);
        // TODO: initialize cmdNode
        _commands.push_back(cmdNode);
    }

    std::vector<std::shared_ptr<FileNode>> BuildFileCompiler::compileInput(
        BuildFile::Input const& input,
        std::vector<std::shared_ptr<FileNode>> included
    ) const {
        std::vector<std::shared_ptr<FileNode>> inputNodes;
        std::shared_ptr<FileNode> fileNode;
        // TODO: use Globber
        std::filesystem::path fileName = _baseDir->name() / input.pathPattern;
        std::shared_ptr<Node> node = _context->nodes().find(fileName);
        if (node != nullptr) {
            fileNode = dynamic_pointer_cast<FileNode>(node);
            if (fileNode == nullptr) throw std::runtime_error("not a file node");
        } else {
            fileNode = std::make_shared<SourceFileNode>(_context, fileName);
        }
        inputNodes.push_back(fileNode);
        return inputNodes;
    }

    std::vector<std::shared_ptr<FileNode>> BuildFileCompiler::compileInputs(
        BuildFile::Inputs const& inputs
    ) const {
        std::vector<std::shared_ptr<FileNode>> inputNodes;
        for (auto const& input : inputs.inputs) {
            std::vector<std::shared_ptr<FileNode>> nodes = compileInput(input, inputNodes);
            inputNodes.insert(inputNodes.end(), nodes.begin(), nodes.end());
        }
        return inputNodes;
    }

    std::string BuildFileCompiler::compileScript(
        BuildFile::Script const& script,
        std::vector<std::shared_ptr<FileNode>> cmdInputs,
        std::vector<std::shared_ptr<GeneratedFileNode>> orderOnlyInputs,
        std::vector<std::shared_ptr<GeneratedFileNode>> outputs
    ) const {
        return script.script;
    }

    std::vector<std::shared_ptr<GeneratedFileNode>> BuildFileCompiler::compileOutput(
        BuildFile::Output const& output,
        std::vector<std::shared_ptr<FileNode>> const& inputs
    ) const {
        std::vector<std::shared_ptr<GeneratedFileNode>> outputNodes;
        return outputNodes;
    }

    std::vector<std::shared_ptr<GeneratedFileNode>> BuildFileCompiler::compileOutputs(
        std::shared_ptr<CommandNode> const& cmdNode,
        std::vector<std::filesystem::path> const& outputPaths,
        std::vector<std::shared_ptr<FileNode>> const& inputs
    ) const {
        std::vector<std::shared_ptr<GeneratedFileNode>> outputNodes;
        // TODO: resolve %B etc
        for (auto const& outputPath : outputPaths) {
            std::shared_ptr<GeneratedFileNode> outputNode;
            std::shared_ptr<Node> node = _context->nodes().find(outputPath);
            if (node != nullptr) {
                outputNode = dynamic_pointer_cast<GeneratedFileNode>(node);
                if (outputNode == nullptr) throw std::runtime_error("not a generated file node");
            } else {
                outputNode = std::make_shared<GeneratedFileNode>(_context, outputPath, cmdNode.get());
            }
            outputNodes.push_back(outputNode);
        }
        return outputNodes;
    }

    void BuildFileCompiler::compileRule(BuildFile::Rule const& rule) {
        std::vector<std::shared_ptr<FileNode>> cmdInputs = compileInputs(rule.cmdInputs);
        std::vector<std::shared_ptr<GeneratedFileNode>> orderOnlyInputs; //TODO
        if (rule.forEach) {
            for (auto const& cmdInput : cmdInputs) {
                std::vector<std::shared_ptr<FileNode>> cmdInputs_({ cmdInput });
                std::vector<std::filesystem::path> outputPaths = expandOutputPaths(rule.outputs, cmdInputs_);
                std::shared_ptr<CommandNode> cmdNode = createCommand(outputPaths);
                addCommand(rule, cmdNode, cmdInputs_, orderOnlyInputs, outputPaths);
            }
        } else {
            std::vector<std::filesystem::path> outputPaths = expandOutputPaths(rule.outputs, cmdInputs);
            std::shared_ptr<CommandNode> cmdNode = createCommand(outputPaths);
            addCommand(rule, cmdNode, cmdInputs, orderOnlyInputs, outputPaths);
        }
    }  
}
