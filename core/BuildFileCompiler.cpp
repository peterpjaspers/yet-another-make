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
            std::shared_ptr<BuildFile::File> const& buildFile)
        : _context(context)
        , _baseDir(baseDir)
    {
        for (auto const& varOrRule : buildFile->variablesAndRules) {
            auto rule = dynamic_cast<BuildFile::Rule*>(varOrRule.get());
            if (rule != nullptr) compileRule(*rule);
        }
    }

    std::vector<std::shared_ptr<FileNode>> BuildFileCompiler::compileInput(
        BuildFile::Input const& input,
        std::vector<std::shared_ptr<FileNode>> included
    ) {
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
    ) {
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
    ) {
        return script.script;
    }

    std::vector<std::shared_ptr<GeneratedFileNode>> BuildFileCompiler::compileOutput(
        BuildFile::Output const& output,
        std::vector<std::shared_ptr<FileNode>> const& inputs
    ) {
        std::vector<std::shared_ptr<GeneratedFileNode>> outputNodes;
        return outputNodes;
    }

    std::vector<std::shared_ptr<GeneratedFileNode>> BuildFileCompiler::compileOutputs(
        BuildFile::Outputs const& outputs,
        std::vector<std::shared_ptr<FileNode>> const& inputs
    ) {
        std::vector<std::shared_ptr<GeneratedFileNode>> outputNodes;
        // TODO: resolve %B etc
        for (auto const& output : outputs.outputs) {
            std::shared_ptr<GeneratedFileNode> fileNode;
            std::filesystem::path fileName = _baseDir->name() / output.path;
            std::shared_ptr<Node> node = _context->nodes().find(fileName);
            if (node != nullptr) {
                fileNode = dynamic_pointer_cast<GeneratedFileNode>(node);
                if (fileNode == nullptr) throw std::runtime_error("not a file node");
            } else {
                fileNode = std::make_shared<GeneratedFileNode>(_context, fileName, nullptr);
            }
            outputNodes.push_back(fileNode);
        }
        return outputNodes;
    }

    void BuildFileCompiler::compileRule(BuildFile::Rule const& rule) {
        std::vector<std::shared_ptr<FileNode>> cmdInputs = compileInputs(rule.cmdInputs);
        if (rule.forEach) {
            for (auto const& cmdInput : cmdInputs) {
                std::vector<std::shared_ptr<FileNode>> cmdInputs({ cmdInput });
                std::vector<std::shared_ptr<GeneratedFileNode>> orderOnlyIn;
                std::vector<std::shared_ptr<GeneratedFileNode>> outputs = compileOutputs(rule.outputs, cmdInputs);
                addCommand(rule, cmdInputs, orderOnlyIn);
            }
        } else {
            std::vector<std::shared_ptr<GeneratedFileNode>> orderOnlyInputs;
            std::vector<std::shared_ptr<GeneratedFileNode>> outputs = compileOutputs(rule.outputs, cmdInputs);
            addCommand(rule, cmdInputs, orderOnlyInputs);
        }
    }

    void BuildFileCompiler::addCommand(
        BuildFile::Rule const& rule,
        std::vector<std::shared_ptr<FileNode>> const& cmdInputs,
        std::vector<std::shared_ptr<GeneratedFileNode>> const& orderOnlyInputs
    ) {
        std::shared_ptr<CommandNode> cmdNode;

        std::vector<std::shared_ptr<GeneratedFileNode>> outputs = compileOutputs(rule.outputs, cmdInputs);
        std::string script = compileScript(rule.script, cmdInputs, orderOnlyInputs, outputs);
        // TODO: what node name to use when there are no outputs
        // E.g. use uid and always create new node
        std::filesystem::path cmdName;
        if (outputs.empty()) {
            cmdName = uniqueCmdName(_context->nodes());
        } else {
            cmdName = outputs[0]->name() / "__cmd";
        }
        std::shared_ptr<Node> node = _context->nodes().find(cmdName);
        cmdNode = dynamic_pointer_cast<CommandNode>(node);
        if (cmdNode == nullptr) {
            cmdNode = std::make_shared<CommandNode>(_context, cmdName);
        }
        // TODO: initialize cmdNode
        _commands.push_back(cmdNode);
    }    
}
