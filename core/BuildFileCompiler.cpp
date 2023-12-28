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
#include "GroupNode.h"

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
        ) {
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

    // Return -1 if stringWithFlags[i] is not a digit
    std::size_t parseOffset(
        std::filesystem::path const& buildFile,
        BuildFile::Node const& node,
        std::string stringWithFlags,
        std::size_t& i
    ) {
        std::size_t nChars = stringWithFlags.length();
        char const* chars = stringWithFlags.data();
        std::size_t offset = -1;
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
                    "Unexpected end after '%" << offset << "' in " << stringWithFlags
                    << " at line " << node.line << " at column " << node.column
                    << " in build file " << buildFile.string()
                    << std::endl;
                throw std::runtime_error(ss.str());
            }
            if (offset == 0) {
                std::stringstream ss;
                ss <<
                    "Offset must >= 1 " << offset << "after '%' " << "in " << stringWithFlags
                    << " at line " << node.line << " at column " << node.column
                    << " in build file " << buildFile.string()
                    << std::endl;
                throw std::runtime_error(ss.str());
            }
            offset -= 1;
        }
        return offset;
    }

    void assertOffset(
        std::filesystem::path const& buildFile,
        BuildFile::Node const& node,
        std::string stringWithFlags,
        std::size_t offset,
        std::size_t maxOffset
    ) {
        if (offset == -1) return;
        if (offset > maxOffset) {
            std::stringstream ss;
            ss <<
                "Too large offset " << offset << "after '%' " << "in " << stringWithFlags
                << " at line " << node.line << " at column " << node.column
                << " in build file " << buildFile.string()
                << std::endl;
            throw std::runtime_error(ss.str());
        }
    }

    std::string compileFlag1(
        std::filesystem::path const& buildFile,
        BuildFile::Node const& node,
        std::string const& stringWithFlags,
        DirectoryNode const* baseDir,
        FileNode const* fileNode,
        char flag
    ) {
        std::filesystem::path inputPath = resolvePath(baseDir, fileNode);
        if (flag == 'f') {
            return inputPath.string();
        } else if (flag == 'b') {
            return inputPath.filename().string();
        } else if (flag == 'B') {
            return inputPath.stem().string();
        } else if (flag == 'e') {
            return inputPath.extension().string();
        } else if (flag == 'd') {
            return inputPath.parent_path().string();
        } else if (flag == 'D') {
            std::filesystem::path dir = inputPath.parent_path().filename();
            return dir.string();
        } else if (flag == 'o') {
            return inputPath.string();
        } else if (flag == 'i') {
            return inputPath.string();
        } else {
            std::stringstream ss;
            ss <<
                "Unkown flag %" << flag << " in " << stringWithFlags
                << " at line " << node.line << " at column " << node.column
                << " in build file " << buildFile.string()
                << std::endl;
            throw std::runtime_error(ss.str());
        }
    }

    template<typename T>
    void compileFlagN(
        std::filesystem::path const& buildFile,
        BuildFile::Node const& node,
        std::string const& stringWithFlags,
        DirectoryNode const* baseDir,
        std::size_t offset,
        std::vector<std::shared_ptr<T>> const& fileNodes,
        char flag,
        std::string& result
     ) {
        if (offset == -1) {
            for (auto const& fileNode : fileNodes) {
                auto path = compileFlag1(buildFile, node, stringWithFlags, baseDir, fileNode.get(), flag);
                result.append(path);
            }
        } else {
            FileNode const* fileNode = fileNodes[offset].get();
            auto path = compileFlag1(buildFile, node, stringWithFlags, baseDir, fileNode, flag);
            result.append(path);
        }
    }

    bool isCmdInputFlag(char c) {
        return (c == 'f' || c == 'b' || c == 'B' || c == 'e' || c == 'd' || c == 'D');
    }

    bool isOrderOnlyInputFlag(char c) {
        return c == 'i';
    }

    bool isOutputFlag(char c) {
        return c == 'o';
    }

    bool containsFlag(std::string const& stringWithFlags, bool (*isFlag)(char)) {
        bool contains = false;
        std::size_t nChars = stringWithFlags.length();
        char const* chars = stringWithFlags.data();
        std::string result;
        for (std::size_t i = 0; !contains && i < nChars; ++i) {
            char c = chars[i];
            if (c == '%') {
                if (++i >= nChars) break;
                c = chars[i];
                if (c == '%') break;
                while (i < nChars && isdigit(c)) c = chars[++i];
                if (i >= nChars) break;
                contains = isFlag(c);
            }
        }
        return contains;
    }

    std::string compilePercentageFlags(
        std::filesystem::path const& buildFile,
        BuildFile::Node const& node,
        DirectoryNode const* baseDir,
        std::string const& stringWithFlags,
        std::vector<std::shared_ptr<FileNode>> const& cmdInputs,
        std::size_t defaultCmdInputOffset,
        std::vector<std::shared_ptr<GeneratedFileNode>> orderOnlyInputs,
        std::vector<std::shared_ptr<GeneratedFileNode>> const& cmdOutputs,
        bool allowOutputFlag // %o
    ) {
        std::size_t nChars = stringWithFlags.length();
        char const* chars = stringWithFlags.data();
        std::string result;
        for (std::size_t i = 0; i < nChars; ++i) {
            if (chars[i] == '%') {
                if (++i >= nChars) {
                    std::stringstream ss;
                    ss <<
                        "Unexpected '%' at end of " << stringWithFlags
                        << " at line " << node.line << " at column " << node.column
                        << " in build file " << buildFile.string()
                        << std::endl;
                    throw std::runtime_error(ss.str());
                }
                if (chars[i] == '%') {
                    result.insert(result.end(), '%');
                    break;
                }
                std::size_t offset = parseOffset(buildFile, node, stringWithFlags, i);
                if (allowOutputFlag && chars[i] == 'o') {
                    assertOffset(buildFile, node, stringWithFlags, offset, cmdOutputs.size());
                    compileFlagN(buildFile, node, stringWithFlags, baseDir, offset, cmdOutputs, chars[i], result);
                } else if (chars[i] == 'i') {
                    assertOffset(buildFile, node, stringWithFlags, offset, orderOnlyInputs.size());
                    compileFlagN(buildFile, node, stringWithFlags, baseDir, offset, orderOnlyInputs, chars[i], result);
                } else {
                    assertOffset(buildFile, node, stringWithFlags, offset, cmdInputs.size()); 
                    if (offset == -1) offset = defaultCmdInputOffset;
                    compileFlagN(buildFile, node, stringWithFlags, baseDir, offset, cmdInputs, chars[i], result);
                } 
            } else {
                result.insert(result.end(), chars[i]);
            }
        }
        return result;
    }

    SourceFileNode* findBuildFileNode(ExecutionContext* context, std::filesystem::path const& buildFile) {
        auto repo = context->findRepositoryContaining(buildFile);
        if (repo != nullptr) {
            auto symBuildFilePath = repo->symbolicPathOf(buildFile);
            auto node = context->nodes().find(symBuildFilePath);
            return dynamic_cast<SourceFileNode*>(node.get());
        }
        return nullptr;
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
        , _buildFile(buildFile.buildFile)
    {
        for (auto const& glob : buildFile.deps.depGlobs) {
            compileGlob(glob);
        }
        for (auto const& varOrRule : buildFile.variablesAndRules) {
            auto rule = dynamic_cast<BuildFile::Rule*>(varOrRule.get());
            if (rule != nullptr) compileRule(*rule);
        }
    }

    std::vector<std::shared_ptr<FileNode>> BuildFileCompiler::compileInputGroup(
        BuildFile::Input const& input
    ) {
        std::shared_ptr<GroupNode> groupNode;
        auto it = _groups.find(input.pathPattern);
        if (it != _groups.end()) groupNode = it->second;
        if (groupNode == nullptr) {
            auto node = _context->nodes().find(input.pathPattern);
            groupNode = dynamic_pointer_cast<GroupNode>(node);
            if (node != nullptr && groupNode == nullptr) {
                std::stringstream ss;
                ss << "In rule at line " << input.line << " in buildfile " << _buildFile.string() << ":" << std::endl;
                ss << "The input group name " << input.pathPattern.string()
                    << " is already in use by node that is not a group," << std::endl;
                throw std::runtime_error(ss.str());
            }
            if (groupNode == nullptr) {
                std::stringstream ss;
                ss << "No such input group name " << input.pathPattern.string() << std::endl;
                ss << "In rule at line " << input.line << " in buildfile " << _buildFile.string() << ":" << std::endl;
                throw std::runtime_error(ss.str());
            }
        }
        std::vector<std::shared_ptr<FileNode>> groupContent;
        for (auto const& n : groupNode->group()) {
            auto fn = dynamic_pointer_cast<FileNode>(n);
            if (fn == nullptr) throw std::runtime_error("Illegal node type in group " + n->name().string());
            groupContent.push_back(fn);
        }
        return groupContent;
    }

    std::vector<std::shared_ptr<FileNode>> BuildFileCompiler::compileInput(
        BuildFile::Input const& input
    ) {
        std::vector<std::shared_ptr<FileNode>> inputNodes;
        std::shared_ptr<FileNode> fileNode;
        if (input.isGroup) {
            inputNodes = compileInputGroup(input);
        } else if (Glob::isGlob(input.pathPattern.string())) {
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
                ss << "In rule at line " << input.line << " in buildfile " << _buildFile.string() << std::endl;
                ss << "Possible causes:" << std::endl;
                ss << "Reference to a non-existing source file, or" << std::endl;
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

    std::vector<std::shared_ptr<GeneratedFileNode>> BuildFileCompiler::compileOrderOnlyInputs(
        BuildFile::Inputs const& inputs
    ) {
        std::vector<std::shared_ptr<FileNode>> inputNodes = compileInputs(inputs);
        std::vector<std::shared_ptr<GeneratedFileNode>> generatedNodes;
        for (auto const& input : inputNodes) {
            auto genNode = dynamic_pointer_cast<GeneratedFileNode>(input);
            if (genNode != nullptr) generatedNodes.push_back(genNode);
        }
        return generatedNodes;
    }

    std::string BuildFileCompiler::compileScript(
        std::filesystem::path const& buildFile,
        BuildFile::Script const& script,
        DirectoryNode const* baseDir,
        std::vector<std::shared_ptr<FileNode>> cmdInputs,
        std::vector<std::shared_ptr<GeneratedFileNode>> orderOnlyInputs,
        std::vector<std::shared_ptr<GeneratedFileNode>> outputs
    ) {
        std::string compiledScript =
            compilePercentageFlags(
                buildFile, script, baseDir, script.script,
                cmdInputs, -1, orderOnlyInputs,
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
                    ss << "In rule at line " << rule.line << " in buildfile " << _buildFile.string() << ":" << std::endl;
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
                ss << "In rule at line " << rule.line << " in buildfile " << _buildFile.string() << ":" << std::endl;
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
        auto base = _baseDir;
        std::filesystem::path pattern = output.path;
        Globber::optimize(base, pattern);
        std::filesystem::path outputPath = compilePercentageFlags(
            _buildFile, output, _baseDir.get(), (base->name() / pattern).string(),
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
                    std::filesystem::path outputPath = compileOutputPath(output, cmdInputs, i);
                    // When output does not contain %-flags then duplicate outputPaths will
                    // be generated when there are multiple cmdInputs. 
                    // E.g. if output==main.exe and inputs are a.obj and b.obj
                    auto it = std::find(outputPaths.begin(), outputPaths.end(), outputPath);
                    if (it == outputPaths.end()) {
                        outputPaths.push_back(outputPath);
                    }
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
            _newCommands.insert({ cmdNode->name(), cmdNode });
        }
        return cmdNode;
    }

    void BuildFileCompiler::compileCommand(
        BuildFile::Rule const& rule,
        std::vector<std::shared_ptr<FileNode>> const& cmdInputs,
        std::vector<std::shared_ptr<GeneratedFileNode>> const& orderOnlyInputs
    ) {
        std::vector<std::filesystem::path> outputPaths = compileOutputPaths(rule.outputs, cmdInputs);
        std::vector<std::filesystem::path> ignoredOutputs = compileIgnoredOutputs(rule.outputs);
        if (outputPaths.empty()) assertScriptHasNoOutputFlag(rule);

        std::shared_ptr<CommandNode> cmdNode = createCommand(outputPaths);
        std::vector<std::shared_ptr<GeneratedFileNode>> outputs = createGeneratedFileNodes(rule, cmdNode, outputPaths);
        std::string script = compileScript(_buildFile, rule.script, _baseDir.get(), cmdInputs, orderOnlyInputs, outputs);
        cmdNode->buildFile(findBuildFileNode(_context, _buildFile));
        cmdNode->ruleLineNr(rule.line);
        cmdNode->workingDirectory(_baseDir);
        cmdNode->cmdInputs(cmdInputs);
        cmdNode->orderOnlyInputs(orderOnlyInputs);
        cmdNode->script(script);
        cmdNode->outputs(outputs);
        cmdNode->ignoreOutputs(ignoredOutputs);
        _commands.insert({ cmdNode->name(), cmdNode });
        compileOutputGroup(rule, outputs);
    }

    void BuildFileCompiler::compileOutputGroup(
        BuildFile::Rule const& rule,
        std::vector<std::shared_ptr<GeneratedFileNode>> const& outputs
    ) {
        if (rule.outputGroup.empty()) return;

        std::shared_ptr<GroupNode> groupNode;
        auto it = _groups.find(rule.outputGroup);
        if (it != _groups.end()) groupNode = it->second;

        if (groupNode == nullptr) {
            auto node = _context->nodes().find(rule.outputGroup);
            groupNode = dynamic_pointer_cast<GroupNode>(node);
            if (node != nullptr && groupNode == nullptr) {
                std::stringstream ss;
                ss << "In rule at line " << rule.line << " in buildfile " << _buildFile.string() << ":" << std::endl;
                ss << "The output group name " << rule.outputGroup.string()
                    << " is already in use by node that is not a group," << std::endl;
                throw std::runtime_error(ss.str());
            }
            if (groupNode == nullptr) {
                groupNode = std::make_shared<GroupNode>(_context, rule.outputGroup);
                _newGroups.insert({ groupNode->name(), groupNode });
            }
            _groups.insert({ groupNode->name(), groupNode });
        }
        std::vector<std::shared_ptr<Node>> group = groupNode->group();
        for (auto const& n : outputs) group.push_back(n);
        groupNode->group(group);
    }


    void BuildFileCompiler::assertScriptHasNoCmdInputFlag(BuildFile::Rule const& rule) const {
        if (containsFlag(rule.script.script, isCmdInputFlag)) {
            std::stringstream ss;
            ss << "In rule at line " << rule.line << " in buildfile " << _buildFile.string() << ":" << std::endl;
            ss << "No cmd input files while script contains percentage flag that operates on cmd input." << std::endl;
            throw std::runtime_error(ss.str());
        }
    }

    void BuildFileCompiler::assertScriptHasNoOrderOnlyInputFlag(BuildFile::Rule const& rule) const {
        if (containsFlag(rule.script.script, isOrderOnlyInputFlag)) {
            std::stringstream ss;
            ss << "In rule at line " << rule.line << " in buildfile " << _buildFile.string() << ":" << std::endl;
            ss << "No order-only input files while script contains percentage flag that operates on order-only input." << std::endl;
            throw std::runtime_error(ss.str());
        }
    }

    void BuildFileCompiler::assertScriptHasNoOutputFlag(BuildFile::Rule const& rule) const {
        if (containsFlag(rule.script.script, isOutputFlag)) {
            std::stringstream ss;
            ss << "In rule at line " << rule.line << " in buildfile " << _buildFile.string() << ":" << std::endl;
            ss << "No output files while script contains percentage flag that operates on output." << std::endl;
            throw std::runtime_error(ss.str());
        }

    }

    void BuildFileCompiler::compileRule(BuildFile::Rule const& rule) {
        std::vector<std::shared_ptr<FileNode>> cmdInputs = compileInputs(rule.cmdInputs);
        std::vector<std::shared_ptr<GeneratedFileNode>> orderOnlyInputs = compileOrderOnlyInputs(rule.orderOnlyInputs);
        if (cmdInputs.empty()) assertScriptHasNoCmdInputFlag(rule);
        if (orderOnlyInputs.empty()) assertScriptHasNoOrderOnlyInputFlag(rule);
        if (rule.forEach) {
            for (auto const& cmdInput : cmdInputs) {
                std::vector<std::shared_ptr<FileNode>> cmdInputs_({ cmdInput });
                compileCommand(rule, cmdInputs_, orderOnlyInputs);
            }
        } else {
            compileCommand(rule, cmdInputs, orderOnlyInputs);
        }
    }  
}
