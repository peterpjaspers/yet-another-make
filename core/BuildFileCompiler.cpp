#include "BuildFileCompiler.h"
#include "ExecutionContext.h"
#include "NodeSet.h"
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
#include <chrono>
#include <ctype.h>

namespace
{
    using namespace YAM;

    std::filesystem::path uidPath() {
        std::stringstream ss;
        ss << rand();
        return std::filesystem::path(ss.str());
    }

    std::filesystem::path uniqueCmdName(
        NodeSet& nodes, 
        std::map<std::filesystem::path, std::shared_ptr<CommandNode>> const& newCommands
    ) {
        std::filesystem::path uid = uidPath();
        while (
            (nodes.find(uid) != nullptr)
            && (newCommands.find(uid) != newCommands.end())
        )  {
            uid = uidPath();
        }
        return uid;
    }

    std::filesystem::path relativePathOf(
        std::filesystem::path const& root,
        std::filesystem::path const& p
    ) {
        auto relPath = p.lexically_relative(root);
        return relPath;
    }

    // If symPath is in same repo as baseDir: return path relative to baseDir
    // Else: return absolute path.
    std::filesystem::path resolvePath(DirectoryNode const* baseDir, FileNode const* fileNode) {
        std::filesystem::path resolved;
        std::shared_ptr<FileRepository> repo = baseDir->repository();
        if (repo->lexicallyContains(fileNode->name())) {
            resolved = relativePathOf(baseDir->name(), fileNode->name());
        } else {
            resolved = fileNode->absolutePath();
        }
        return resolved;
    }

    std::size_t parseOffset(
        BuildFile::Node const& node,
        std::string string,
        std::size_t& i,
        std::size_t defaultOffset,
        std::size_t maxOffset
    ) {
        std::size_t nChars = string.length();
        char const* chars = string.data();
        std::size_t offset = defaultOffset;
        if (isdigit(chars[i])) {
            offset = 0;
            std::size_t mul = 1;
            do {
                offset = (offset * mul) + (chars[i] - '0');
                mul *= 10;
            } while (++i < nChars && isdigit(chars[i]));
            if (i >= nChars) {
                std::stringstream ss;
                ss <<
                    "Unexpected end after '%" << offset << "' in " << string
                    << " at line " << node.line << " at column " << node.column
                    << " in build file " << node.buildFile.string()
                    << std::endl;
                throw std::runtime_error(ss.str());
            }
            if (offset == 0) {
                std::stringstream ss;
                ss <<
                    "Offset must >= 1 " << offset << "after '%' " << "in " << string
                    << " at line " << node.line << " at column " << node.column
                    << " in build file " << node.buildFile.string()
                    << std::endl;
                throw std::runtime_error(ss.str());
            }
            if (offset > maxOffset) {
                std::stringstream ss;
                ss <<
                    "Too large offset " << offset << "after '%' " << "in " << string
                    << " at line " << node.line << " at column " << node.column
                    << " in build file " << node.buildFile.string()
                    << std::endl;
                throw std::runtime_error(ss.str());
            }
        }
        return offset-1;
    }

    std::string compilePercentageFlags(
        BuildFile::Node const& node,
        DirectoryNode const* baseDir,
        std::string const& string,
        std::vector<std::shared_ptr<FileNode>> const& cmdInputs,
        std::size_t defaultCmdInputOffset,
        std::vector<std::shared_ptr<GeneratedFileNode>> orderOnlyInputs,
        std::vector<std::shared_ptr<GeneratedFileNode>> const& cmdOutputs,
        bool allowOutputFlag // %o
    ) {
        std::size_t nChars = string.length();
        char const* chars = string.data();
        std::string result;
        for (std::size_t i = 0; i < nChars; ++i) {
            if (chars[i] == '%') {
                if (++i >= nChars) {
                    std::stringstream ss;
                    ss <<
                        "Unexpected '%' at end of " << string
                        << " at line " << node.line << " at column " << node.column
                        << " in build file " << node.buildFile.string()
                        << std::endl;
                    throw std::runtime_error(ss.str());
                }
                if (chars[i] == '%') {
                    result.insert(result.end(), '%');
                    break;
                } 
                if (allowOutputFlag && chars[i] == 'o') {
                    std::size_t offset = parseOffset(node, string, i, 1, cmdOutputs.size());
                    result.append(resolvePath(baseDir, cmdOutputs[offset].get()).string());
                } else if (chars[i] == 'i') {
                    std::size_t offset = parseOffset(node, string, i, 1, orderOnlyInputs.size());
                    result.append(resolvePath(baseDir, orderOnlyInputs[offset].get()).string());
                } else {
                    std::size_t offset = parseOffset(node, string, i, defaultCmdInputOffset, cmdInputs.size());
                    std::filesystem::path inputPath = resolvePath(baseDir, cmdInputs[offset].get()).string();
                    if (chars[i] == 'f') {
                        result.append(inputPath.string());
                    } else if (chars[i] == 'b') {
                        result.append(inputPath.filename().string());
                    } else if (chars[i] == 'B') {
                        result.append(inputPath.stem().string());
                    } else if (chars[i] == 'e') {
                        result.append(inputPath.extension().string());
                    } else if (chars[i] == 'd') {
                        result.append(inputPath.parent_path().string());
                    } else if (chars[i] == 'D') {
                        std::filesystem::path dir = inputPath.parent_path().filename();
                        result.append(dir.string());
                    } else {
                        std::stringstream ss;
                        ss <<
                            "Unkown flag %" << chars[i] << " in " << string
                            << " at line " << node.line << " at column " << node.column
                            << " in build file " << node.buildFile.string()
                            << std::endl;
                        throw std::runtime_error(ss.str());
                    }
                }
            } else {
                result.insert(result.end(), chars[i]);
            }
        }
        return result;
    }
}

namespace YAM {
    BuildFileCompiler::BuildFileCompiler(
            ExecutionContext* context,
            std::shared_ptr<DirectoryNode> const& baseDir,
            BuildFile::File const& buildFile,
            std::filesystem::path const& globNameSpace)
        : _context(context)
        , _baseDir(baseDir)
        , _globNameSpace(globNameSpace)
    {
        for (auto const& glob : buildFile.deps.depGlobs) {
            compileGlob(glob);
        }
        for (auto const& varOrRule : buildFile.variablesAndRules) {
            auto rule = dynamic_cast<BuildFile::Rule*>(varOrRule.get());
            if (rule != nullptr) compileRule(*rule);
        }
    }

    std::shared_ptr<CommandNode> BuildFileCompiler::createCommand(
        std::vector<std::filesystem::path> const& outputPaths)
    {
        std::shared_ptr<CommandNode> cmdNode;
        std::filesystem::path cmdName;
        if (outputPaths.empty()) {
            cmdName = uniqueCmdName(_context->nodes(), _newCommands);
        } else {
            cmdName = outputPaths[0] / "__cmd";
        }
        std::shared_ptr<Node> node = _context->nodes().find(cmdName);

        cmdNode = dynamic_pointer_cast<CommandNode>(node);
        if (cmdNode == nullptr) {
            cmdNode = std::make_shared<CommandNode>(_context, cmdName);
            _newCommands.insert({ cmdNode->name(), cmdNode});
        }
        return cmdNode;
    }

    void BuildFileCompiler::addCommand(
        BuildFile::Rule const& rule,
        std::vector<std::shared_ptr<FileNode>> const& cmdInputs,
        std::vector<std::shared_ptr<GeneratedFileNode>> const& orderOnlyInputs
    ) {
        auto repo = _context->findRepositoryContaining(rule.buildFile);
        auto symBuildFilePath = repo->symbolicPathOf(rule.buildFile);
        auto buildFileNode = dynamic_pointer_cast<SourceFileNode>(_context->nodes().find(symBuildFilePath));

        std::vector<std::filesystem::path> outputPaths = compileOutputPaths(rule.outputs, cmdInputs);
        std::vector<std::filesystem::path> ignoredOutputs = compileIgnoredOutputs(rule.outputs);

        std::shared_ptr<CommandNode> cmdNode = createCommand(outputPaths);
        std::vector<std::shared_ptr<GeneratedFileNode>> outputs = createGeneratedFileNodes(rule, cmdNode, outputPaths);
        std::string script = compileScript(rule.script, _baseDir.get(), cmdInputs, orderOnlyInputs, outputs);
        cmdNode->buildFile(buildFileNode.get());
        cmdNode->ruleLineNr(rule.line);
        cmdNode->workingDirectory(_baseDir);
        cmdNode->cmdInputs(cmdInputs);
        cmdNode->orderOnlyInputs(orderOnlyInputs);
        cmdNode->script(script);
        cmdNode->outputs(outputs);
        cmdNode->ignoreOutputs(ignoredOutputs);
        _commands.insert({ cmdNode->name(), cmdNode });
    }

    std::vector<std::shared_ptr<FileNode>> BuildFileCompiler::compileInput(
        BuildFile::Input const& input
    ) {
        std::vector<std::shared_ptr<FileNode>> inputNodes;
        std::shared_ptr<FileNode> fileNode;
        if (Glob::isGlob(input.pathPattern.string())) {
            std::shared_ptr<GlobNode> globNode = compileGlob(input.pathPattern);
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
            auto optimizedBaseDir = _baseDir;
            auto optimizedPattern = input.pathPattern;
            Globber::optimize(optimizedBaseDir, optimizedPattern);
            std::filesystem::path inputPath(optimizedBaseDir->name() / optimizedPattern);
            auto match = _context->nodes().find(inputPath);
            fileNode = dynamic_pointer_cast<FileNode>(match);
            if (fileNode == nullptr) {
                std::stringstream ss;
                ss << input.pathPattern.string() << ": no such input file." << std::endl;
                ss << "In rule at line " << input.line << " in buildfile " << input.buildFile.string() << std::endl;
                ss << "Possible causes:" << std::endl;
                ss << "Reference to a non-exiting source file, or" << std::endl;
                ss << "Misspelled name of a source file or generated file, or" << std::endl;
                ss << "Reference to a generated file that has not yet been defined." << std::endl;
                ss << "If the generated file is defined in a rule in this buildfile " << std::endl;
                ss << "then move that rule to a line above the offending rule." << std::endl;
                ss << "If the generated file is defined in a rule in another buildfile" << std::endl;
                ss << "then declare the dependency on that other buildfile in this buildfile." << std::endl;
                throw std::runtime_error(ss.str());
            }
            inputNodes.push_back(fileNode);
        }
        return inputNodes;
    }

    std::shared_ptr<GlobNode> BuildFileCompiler::compileGlob(
        std::filesystem::path const& pattern
    ) {
        auto optimizedBaseDir = _baseDir;
        auto optimizedPattern = pattern;
        Globber::optimize(optimizedBaseDir, optimizedPattern);
        std::filesystem::path globName(_globNameSpace / optimizedBaseDir->name() / optimizedPattern);
        auto globNode = dynamic_pointer_cast<GlobNode>(_context->nodes().find(globName));
        if (globNode == nullptr) {  
            auto it = _newGlobs.find(globName);
            if (it != _newGlobs.end()) globNode = it->second;
        }
        if (globNode == nullptr) {
            globNode = std::make_shared<GlobNode>(_context, globName);
            globNode->baseDirectory(optimizedBaseDir);
            globNode->pattern(optimizedPattern);
            globNode->initialize();
            _newGlobs.insert({ globNode->name(), globNode });
        }
        _globs.insert({ globNode->name(), globNode });
        return globNode;
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
        DirectoryNode const* baseDir,
        std::vector<std::shared_ptr<FileNode>> cmdInputs,
        std::vector<std::shared_ptr<GeneratedFileNode>> orderOnlyInputs,
        std::vector<std::shared_ptr<GeneratedFileNode>> outputs
    ) {
        std::string compiledScript =
            compilePercentageFlags(
                script, baseDir, script.script,
                cmdInputs, 1, orderOnlyInputs,
                outputs, true);
        return compiledScript;
    }

    std::shared_ptr<GeneratedFileNode> BuildFileCompiler::createGeneratedFileNode(
        BuildFile::Rule const& rule,
        std::shared_ptr<CommandNode> const& cmdNode,
        std::filesystem::path const& outputPath
    ) {
        std::shared_ptr<GeneratedFileNode> outputNode;
        std::shared_ptr<Node> node = _context->nodes().find(outputPath);
        if (node == nullptr) {
            auto it = _newOutputs.find(outputPath);
            if (it != _newOutputs.end()) {
                node = it->second;
            }
        }
        if (node != nullptr) {
            outputNode = dynamic_pointer_cast<GeneratedFileNode>(node);
            if (outputNode == nullptr) {
                auto sourceFileNode = dynamic_pointer_cast<SourceFileNode>(node);
                if (sourceFileNode == nullptr) {
                    throw std::runtime_error("No such file node: " + node->name().string());
                } else {
                    std::stringstream ss;
                    ss << "In rule at line " << rule.line << " in buildfile " << rule.buildFile.string() << ":" << std::endl;
                    ss << "Output file is either a source file or a stale output file: " << node->name().string() << std::endl;
                    ss << "If a source file: fix the rule definition." << std::endl;
                    ss << "If a stale output file: delete it." << std::endl;
                    ss << "Note: output files become stale when you delete yam's buildstate." << std::endl;
                    ss << "When deleting yam's buildstate also delete all output files before running a build." << std::endl;
                    ss << "In all other cases you have found a bug in yam" << std::endl;
                   throw std::runtime_error(ss.str());
                }
            }
            if (outputNode->producer() != cmdNode.get()) {
                auto producer = outputNode->producer();
                std::stringstream ss;
                ss << "In rule at line " << rule.line << " in buildfile " << rule.buildFile.string() << ":" << std::endl;
                ss << "Output file " << outputNode->name().string()
                    << " already defined at rule at line " << producer->ruleLineNr() 
                    << " in buildfile " << producer->buildFile()->absolutePath() << std::endl;
                ss << "Fix rule to remove duplicate output file." << std::endl;
                throw std::runtime_error(ss.str());
            }
        } else {
            outputNode = std::make_shared<GeneratedFileNode>(_context, outputPath, cmdNode.get());
            _newOutputs.insert({ outputPath, outputNode });
        }
        _outputs.insert({ outputNode->name(), outputNode });
        return outputNode;
    }

    std::vector<std::shared_ptr<GeneratedFileNode>> BuildFileCompiler::createGeneratedFileNodes(
        BuildFile::Rule const& rule,
        std::shared_ptr<CommandNode> const& cmdNode,
        std::vector<std::filesystem::path> const& outputPaths
    ) {
        std::vector<std::shared_ptr<GeneratedFileNode>> outputNodes;
        for (auto const& outputPath : outputPaths) {
            std::shared_ptr<GeneratedFileNode> outputNode = createGeneratedFileNode(rule, cmdNode, outputPath);
            outputNodes.push_back(outputNode);
        }
        return outputNodes;
    }

    std::filesystem::path BuildFileCompiler::compileOutputPath(
        BuildFile::Output const& output,
        std::vector<std::shared_ptr<FileNode>> const& cmdInputs,
        std::size_t defaultInputOffset
    ) const {
        static std::vector<std::shared_ptr<GeneratedFileNode>> emptyOutputs; 
        std::vector<std::shared_ptr<GeneratedFileNode>> emptyOrderOnlyInputs;
        std::filesystem::path outputPath =
            compilePercentageFlags(
                output, _baseDir.get(), output.path.string(),
                cmdInputs, defaultInputOffset, emptyOrderOnlyInputs,
                emptyOutputs, false);
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
                    std::filesystem::path outputPath = compileOutputPath(output, cmdInputs, i+1);
                    outputPaths.push_back(outputPath);
                }
            }
        }
        return outputPaths;
    }

    std::vector<std::filesystem::path> BuildFileCompiler::compileIgnoredOutputs(
        BuildFile::Outputs const& outputs
    ) const {
        std::vector<std::filesystem::path> ignoredOutputs;
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
