#include "BuildFileCompiler.h"
#include "ExecutionContext.h"
#include "FileNode.h"
#include "SourceFileNode.h"
#include "GeneratedFileNode.h"
#include "DirectoryNode.h"
#include "CommandNode.h"
#include "FileRepository.h"
#include "Globber.h"
#include "Glob.h"
#include "GlobNode.h"

#include <sstream>
#include <ctype.h>

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

    std::size_t parseInputOffset(
        BuildFile::Output const& output,
        std::size_t& i,
        std::size_t defaultOffset,
        std::size_t maxOffset
    ) {
        std::size_t nChars = output.path.length();
        char const* chars = output.path.data();
        std::size_t inputOffset = defaultOffset;
        if (isdigit(chars[i])) {
            inputOffset = 0;
            std::size_t mul = 1;
            do {
                inputOffset = (inputOffset * mul) + (chars[i] - '0');
                mul *= 10;
            } while (++i < nChars && isdigit(chars[i]));
            if (i >= nChars) {
                std::stringstream ss;
                ss <<
                    "Unexpected end after '%" << inputOffset << "' in " << output.path
                    << " at line " << output.line << " at column " << output.column
                    << " in build file " << output.buildFile.string()
                    << std::endl;
                throw std::runtime_error(ss.str());
            }
            if (inputOffset >= maxOffset) {
                std::stringstream ss;
                ss <<
                    "Too large offset " << inputOffset << "after '%' " << "in " << output.path
                    << " at line " << output.line << " at column " << output.column
                    << " in build file " << output.buildFile.string()
                    << std::endl;
                throw std::runtime_error(ss.str());
            }
        }
        return inputOffset;
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
            _context->nodes().add(cmdNode);
        }
        return cmdNode;
    }

    void BuildFileCompiler::addCommand(
        BuildFile::Rule const& rule,
        std::vector<std::shared_ptr<FileNode>> const& cmdInputs,
        std::vector<std::shared_ptr<GeneratedFileNode>> const& orderOnlyInputs
    ) {
        std::vector<std::filesystem::path> outputPaths = compileOutputPaths(rule.outputs, cmdInputs);
        std::vector<std::string> ignoredOutputs = compileIgnoredOutputs(rule.outputs);

        std::shared_ptr<CommandNode> cmdNode = createCommand(outputPaths);
        std::vector<std::shared_ptr<GeneratedFileNode>> outputs = compileOutputs(cmdNode, outputPaths, cmdInputs);
        std::string script = compileScript(rule.script, cmdInputs, orderOnlyInputs, outputs);
        cmdNode->workingDirectory(_baseDir);
        cmdNode->cmdInputs(cmdInputs);
        cmdNode->orderOnlyInputs(orderOnlyInputs);
        cmdNode->script(script);
        cmdNode->outputs(outputs);
        cmdNode->ignoreOutputs(ignoredOutputs);
        _commands.push_back(cmdNode);
    }

    std::vector<std::shared_ptr<FileNode>> BuildFileCompiler::compileInput(
        BuildFile::Input const& input
    ) {
        std::vector<std::shared_ptr<FileNode>> inputNodes;
        std::shared_ptr<FileNode> fileNode;
        auto optimizedBaseDir = _baseDir;
        auto optimizedPattern = input.pathPattern;
        Globber::optimize(optimizedBaseDir, optimizedPattern);
        if (Glob::isGlob(optimizedPattern.string())) {
            std::filesystem::path globName(optimizedBaseDir->name() / optimizedPattern);
            auto globNode = dynamic_pointer_cast<GlobNode>(_context->nodes().find(globName));
            if (globNode == nullptr) {
                globNode = std::make_shared<GlobNode>(_context, globName);
                globNode->baseDirectory(optimizedBaseDir);
                globNode->pattern(optimizedPattern);
                globNode->initialize();
                _context->nodes().add(globNode);
            }
            _globs.insert({ globName, globNode });
            for (auto const& match : globNode->matches()) {
                fileNode = dynamic_pointer_cast<FileNode>(match);
                if (fileNode == nullptr) throw std::runtime_error("not a file node");
                inputNodes.push_back(fileNode);
            }
        } else {
            // Non-glob inputs can refer to source or generated files.
            // Using GlobNode is inefficient in case of source file and not 
            // supported in case of generated file. Instead do a direct lookup 
            // in context.
            std::filesystem::path inputPath(optimizedBaseDir->name() / optimizedPattern);
            auto match = _context->nodes().find(inputPath);
            fileNode = dynamic_pointer_cast<FileNode>(match);
            if (fileNode == nullptr) throw std::runtime_error(input.pathPattern.string() + ": no such file");
            inputNodes.push_back(fileNode);
        }
        return inputNodes;
    }

    void erase(
        std::vector<std::shared_ptr<FileNode>>& nodes,
        std::vector<std::shared_ptr<FileNode>> const& toErase
    ) {
        for (auto const& n : toErase) {
            auto it = std::find(nodes.begin(), nodes.end(), n);
            if (it != nodes.end()) nodes.erase(it);
        }
    }

    std::vector<std::shared_ptr<FileNode>> BuildFileCompiler::compileInputs(
        BuildFile::Inputs const& inputs
    ) {
        std::vector<std::shared_ptr<FileNode>> inputNodes;
        for (auto const& input : inputs.inputs) {
            std::vector<std::shared_ptr<FileNode>> nodes = compileInput(input);
            if (input.exclude) {
                erase(inputNodes, nodes);
            } else {
                inputNodes.insert(inputNodes.end(), nodes.begin(), nodes.end());
            }
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
        for (auto const& outputPath : outputPaths) {
            std::shared_ptr<GeneratedFileNode> outputNode;
            std::shared_ptr<Node> node = _context->nodes().find(outputPath);
            if (node != nullptr) {
                outputNode = dynamic_pointer_cast<GeneratedFileNode>(node);
                if (outputNode == nullptr) throw std::runtime_error("not a generated file node");
            } else {
                outputNode = std::make_shared<GeneratedFileNode>(_context, outputPath, cmdNode.get());
                _context->nodes().add(outputNode);
            }
            outputNodes.push_back(outputNode);
        }
        return outputNodes;
    }

    std::filesystem::path BuildFileCompiler::compileOutputPath(
        BuildFile::Output const& output,
        std::vector<std::shared_ptr<FileNode>> const& cmdInputs,
        std::size_t defaultOffset
    ) const {
        std::vector<std::filesystem::path> outputPaths;
        std::size_t nChars = output.path.length();
        char const *chars = output.path.data();
        std::string outputPath;
        for (std::size_t i = 0; i < nChars; ++i) {
            if (chars[i] == '%') {
                if (++i >= nChars) {
                    std::stringstream ss;
                    ss << 
                        "Unexpected '%' at end of " << output.path 
                        << " at line " << output.line << " at column " << output.column
                        << " in build file " << output.buildFile.string()
                        << std::endl;
                    throw std::runtime_error(ss.str());
                }
                if (chars[i] == '%') {
                    outputPath.insert(outputPath.end(), '%');
                    break;
                }
                std::size_t inputOffset = parseInputOffset(output, i, defaultOffset, cmdInputs.size());
                std::filesystem::path inputPath = cmdInputs[inputOffset]->name();
                if (chars[i] == 'f') {
                    outputPath.append(inputPath.string());
                } else if (chars[i] == 'b') {
                    outputPath.append(inputPath.filename().string());
                } else if (chars[i] == 'B') {
                    outputPath.append(inputPath.stem().string());
                } else if (chars[i] == 'e') {
                    outputPath.append(inputPath.extension().string());
                } else if (chars[i] == 'd') {
                    outputPath.append(inputPath.parent_path().string());
                } else if (chars[i] == 'D') {
                    std::filesystem::path dir = inputPath.parent_path().filename();
                    outputPath.append(dir.string());
                } else {
                    std::stringstream ss;
                    ss <<
                        "Unkown flag %" << chars[i] << " in " << output.path
                        << " at line " << output.line << " at column " << output.column
                        << " in build file " << output.buildFile.string()
                        << std::endl;
                    throw std::runtime_error(ss.str());
                }
            } else {
                outputPath.insert(outputPath.end(), chars[i]);
            }
        }
        if (!FileRepository::isSymbolicPath(outputPath)) return _baseDir->name() / outputPath;
        return std::filesystem::path(outputPath);
    }

    std::vector<std::filesystem::path> BuildFileCompiler::compileOutputPaths(
        BuildFile::Outputs const& outputs,
        std::vector<std::shared_ptr<FileNode>> const& cmdInputs
    ) const {
        std::vector<std::filesystem::path> outputPaths;
        for (auto const& output : outputs.outputs) {
            if (!output.ignore) {
                for (std::size_t i = 0; i < cmdInputs.size(); ++i) {
                    std::filesystem::path outputPath = compileOutputPath(output, cmdInputs, i);
                    outputPaths.push_back(outputPath);
                }
            }
        }
        return outputPaths;
    }

    std::vector<std::string> BuildFileCompiler::compileIgnoredOutputs(
        BuildFile::Outputs const& outputs
    ) const {
        std::vector<std::string> ignoredOutputs;
        for (auto const& output : outputs.outputs) {
            if (output.ignore) {
                ignoredOutputs.push_back(output.path);
            }
        }
        return ignoredOutputs;
    }

    void BuildFileCompiler::compileRule(BuildFile::Rule const& rule) {
        std::vector<std::shared_ptr<FileNode>> cmdInputs = compileInputs(rule.cmdInputs);
        std::vector<std::shared_ptr<GeneratedFileNode>> orderOnlyInputs; //TODO
        if (rule.forEach) {
            for (auto const& cmdInput : cmdInputs) {
                std::vector<std::shared_ptr<FileNode>> cmdInputs_({ cmdInput });
                addCommand(rule, cmdInputs_, orderOnlyInputs);
            }
        } else {
            addCommand(rule, cmdInputs, orderOnlyInputs);
        }
    }  
}
