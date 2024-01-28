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
#include "AcyclicTrail.h"

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
    std::filesystem::path resolvePath(DirectoryNode const* baseDir, Node const* fileNode) {
        std::filesystem::path resolved;
        std::shared_ptr<FileRepository> repo = baseDir->repository();
        if (repo->lexicallyContains(fileNode->name())) {
            resolved = relativePathOf(baseDir->name(), fileNode->name());
        } else {
            resolved = fileNode->absolutePath();
        }
        return resolved;
    }

    template<typename TNode>
    std::vector<std::filesystem::path> relativePathsOf(
        DirectoryNode const* baseDir,
        std::vector<std::shared_ptr<TNode>> const& nodes
    ) {
        std::vector<std::filesystem::path> relPaths;
        for (auto const& node : nodes) {
            relPaths.push_back(node->name().lexically_relative(baseDir->name()));
        }
        return relPaths;
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
        std::size_t columnOffset,
        std::size_t offset,
        std::size_t maxOffset
    ) {
        if (offset == -1) return;
        if (offset >= maxOffset) {
            std::stringstream ss;
            ss <<
                "Too large offset " << offset+1
                << " at line " << node.line << " at column " << node.column + columnOffset
                << " in build file " << buildFile.string()
                << std::endl;
            throw std::runtime_error(ss.str());
        }
    }

    std::string compileFlag1(
        std::filesystem::path const& buildFile,
        BuildFile::Node const& node,
        DirectoryNode const* baseDir,
        std::filesystem::path inputPath,
        char flag
    ) {
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
                "Unknown flag %" << flag 
                << " at line " << node.line << " at column " << node.column
                << " in build file " << buildFile.string()
                << std::endl;
            throw std::runtime_error(ss.str());
        }
    }

    void compileFlagN(
        std::filesystem::path const& buildFile,
        BuildFile::Node const& node,
        DirectoryNode const* baseDir,
        std::size_t offset,
        std::vector<std::filesystem::path> const& filePaths,
        char flag,
        std::string& result
     ) {
        if (offset == -1) {
            std::size_t nFiles = filePaths.size();
            for (std::size_t i = 0; i < nFiles; ++i) {
                auto path = compileFlag1(buildFile, node, baseDir, filePaths[i], flag);
                result.append(path);
                if (i < nFiles-1) result.append(" ");
            }
        } else {
            std::filesystem::path filePath = filePaths[offset];
            auto path = compileFlag1(buildFile, node, baseDir, filePath, flag);
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

    void assertValidFlag(
        std::filesystem::path const& buildFile,
        BuildFile::Node const& node,
        std::size_t columnOffset,
        char flag
    ) {
        static std::vector<char> flags = { 'f','b','B', 'e', 'd', 'D', 'i', 'o' };
        if (flags.end() == std::find(flags.begin(), flags.end(), flag)) {
            std::size_t column = node.column + columnOffset;
            std::stringstream ss;
                ss <<
                "Unknown flag %" << flag
                << " at line " << node.line << " at column " << column
                << " in build file " << buildFile.string()
                << std::endl;
                throw std::runtime_error(ss.str());
        }
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
        std::vector<std::shared_ptr<Node>> const& cmdInputs,
        std::size_t defaultCmdInputOffset,
        std::vector<std::shared_ptr<Node>> orderOnlyInputs,
        std::vector<std::shared_ptr<GeneratedFileNode>> const& cmdOutputs,
        bool allowOutputFlag // %o
    ) {
        std::vector<std::filesystem::path> cmdInputPaths = relativePathsOf(baseDir, cmdInputs);
        std::vector<std::filesystem::path> orderOnlyInputPaths = relativePathsOf(baseDir, orderOnlyInputs);
        std::vector<std::filesystem::path> cmdOutputPaths = relativePathsOf(baseDir, cmdOutputs);

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

                std::size_t oidx = i;
                std::size_t offset = parseOffset(buildFile, node, stringWithFlags, i);
                std::size_t columnOffset = i;
                if (allowOutputFlag) {
                    // stringWithFlags is the rule command. Correct column offset for start
                    // token of command string "|>".
                    columnOffset = i + 2;
                }
                assertValidFlag(buildFile, node, columnOffset, chars[i]);
                if (allowOutputFlag && chars[i] == 'o') {
                    assertOffset(buildFile, node, oidx, offset, cmdOutputPaths.size());
                    compileFlagN(buildFile, node, baseDir, offset, cmdOutputPaths, chars[i], result);
                } else if (chars[i] == 'i') {
                    assertOffset(buildFile, node, oidx, offset, orderOnlyInputPaths.size());
                    compileFlagN(buildFile, node, baseDir, offset, orderOnlyInputPaths, chars[i], result);
                } else {
                    assertOffset(buildFile, node, oidx, offset, cmdInputPaths.size());
                    if (offset == -1) offset = defaultCmdInputOffset;
                    compileFlagN(buildFile, node, baseDir, offset, cmdInputPaths, chars[i], result);
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

    std::shared_ptr<GroupNode> BuildFileCompiler::compileGroupNode(
        BuildFile::Node const& rule,
        std::filesystem::path const& groupName
    ) {
        auto optimizedBaseDir = _baseDir;
        auto optimizedPattern = groupName;
        Globber::optimize(optimizedBaseDir, optimizedPattern);
        std::filesystem::path groupPath(optimizedBaseDir->name() / optimizedPattern);

        std::shared_ptr<GroupNode> groupNode;
        auto it = _outputGroups.find(groupPath);
        if (it != _outputGroups.end()) groupNode = it->second;
        if (groupNode == nullptr) {
            auto node = _context->nodes().find(groupPath);
            groupNode = dynamic_pointer_cast<GroupNode>(node);
            if (node != nullptr && groupNode == nullptr) {
                std::stringstream ss;
                ss << "In rule at line " << rule.line << " in buildfile " << _buildFile.string() << ":" << std::endl;
                ss << "The input group name " << groupName.string()
                    << " is already in use by a node that is not a group," << std::endl;
                throw std::runtime_error(ss.str());
            }
            if (groupNode == nullptr) {
                groupNode = std::make_shared<GroupNode>(_context, groupPath);
                _context->nodes().add(groupNode);
                _newGroups.insert({ groupNode->name(), groupNode});
            }
        }
        return groupNode;
    }

    std::vector<std::shared_ptr<GeneratedFileNode>>& BuildFileCompiler::compileBin(
        BuildFile::Node const& rule,
        std::filesystem::path const& binPath
    ) {
        auto it = _bins.find(binPath);
        if (it == _bins.end()) {
            _bins.insert({ binPath, std::vector<std::shared_ptr<GeneratedFileNode>>()});
        }
        return _bins[binPath];
    }

    std::shared_ptr<GroupNode> BuildFileCompiler::compileInputGroup(
        BuildFile::Input const& input
    ) {
        return compileGroupNode(input, input.path);
    }

    std::vector<std::shared_ptr<GeneratedFileNode>>& BuildFileCompiler::compileInputBin(
        BuildFile::Input const& input
    ) {
        auto it = _bins.find(input.path);
        if (it == _bins.end()) {
            std::stringstream ss;
            ss << input.path.string() << ": no such input bin." << std::endl;
            ss << "In rule at line " << input.line << " in buildfile " << _buildFile.string() << std::endl;
            ss << "A bin can only be used as input when it was defined as" << std::endl;
            ss << " output bin of an earlier defined rule." << std::endl;
            throw std::runtime_error(ss.str());
        }
        return (*it).second;
    }

    std::shared_ptr<FileNode> BuildFileCompiler::compileInputPath(
        BuildFile::Input const&input
    ) {
        auto optimizedBaseDir = _baseDir;
        auto optimizedPattern = input.path;
        Globber::optimize(optimizedBaseDir, optimizedPattern);
        std::filesystem::path inputPath(optimizedBaseDir->name() / optimizedPattern);
        auto fileNode = dynamic_pointer_cast<FileNode>(_context->nodes().find(inputPath));
        if (fileNode == nullptr) {
            auto it = _outputs.find(inputPath);
            if (it != _outputs.end()) fileNode = dynamic_pointer_cast<FileNode>(it->second);
        }
        if (fileNode == nullptr || fileNode->state() == Node::State::Deleted) {
            std::stringstream ss;
            ss << input.path.string() << ": no such input file." << std::endl;
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
        return fileNode;
    }

    std::vector<std::shared_ptr<Node>> BuildFileCompiler::compileInput(
        BuildFile::Input const& input
    ) {
        std::vector<std::shared_ptr<Node>> inputNodes;
        std::shared_ptr<FileNode> fileNode;
        if (input.pathType == BuildFile::PathType::Group) {
            inputNodes.push_back(compileInputGroup(input));
        } else if (input.pathType == BuildFile::PathType::Bin) {
            std::vector<std::shared_ptr<GeneratedFileNode>>& binContent = compileInputBin(input);
            for (auto const& node : binContent) {
                fileNode = dynamic_pointer_cast<FileNode>(node);
                inputNodes.push_back(fileNode);
            }
        } else if (input.pathType == BuildFile::PathType::Glob) {
            std::shared_ptr<GlobNode> globNode = compileGlob(input.path);
            for (auto const& match : globNode->matches()) {
                fileNode = dynamic_pointer_cast<FileNode>(match);
                if (fileNode == nullptr) throw std::runtime_error("not a file node");
                inputNodes.push_back(fileNode);
            }
        } else if (input.pathType == BuildFile::PathType::Path) {
            // Using GlobNode is inefficient in case of a single input path.
            std::shared_ptr<FileNode> fileNode = compileInputPath(input);
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
        std::vector<std::shared_ptr<Node>>& nodes,
        std::vector<std::shared_ptr<Node>> const& toErase
    ) {
        for (auto const& n : toErase) {
            auto it = std::find(nodes.begin(), nodes.end(), n);
            if (it != nodes.end()) nodes.erase(it);
        }
    }

    std::vector<std::shared_ptr<Node>> BuildFileCompiler::compileInputs(
        BuildFile::Inputs const& inputs
    ) {
        std::vector<std::shared_ptr<Node>> inputNodes;
        for (auto const& input : inputs.inputs) {
            std::vector<std::shared_ptr<Node>> nodes = compileInput(input);
            if (input.exclude) {
                erase(inputNodes, nodes);
            } else {
                inputNodes.insert(inputNodes.end(), nodes.begin(), nodes.end());
            }
        }
        return inputNodes;
    }

    std::vector<std::shared_ptr<Node>> BuildFileCompiler::compileOrderOnlyInputs(
        BuildFile::Inputs const& inputs
    ) {
        return compileInputs(inputs);
    }

    std::string BuildFileCompiler::compileScript(
        std::filesystem::path const& buildFile,
        BuildFile::Script const& script,
        DirectoryNode const* baseDir,
        std::vector<std::shared_ptr<Node>> cmdInputs,
        std::vector<std::shared_ptr<Node>> orderOnlyInputs,
        std::vector<std::shared_ptr<GeneratedFileNode>> const& outputs
    ) {
        std::string compiledScript =
            compilePercentageFlags(
                buildFile, script, baseDir, script.script,
                cmdInputs, -1, orderOnlyInputs,
                outputs, true);
        return compiledScript;
    }
    bool BuildFileCompiler::containsCmdInputFlag(std::string const& script) {
        return containsFlag(script, isCmdInputFlag);
    }

    bool BuildFileCompiler::containsOrderOnlyInputFlag(std::string const& script) {
        return containsFlag(script, isOrderOnlyInputFlag);
    }

    bool BuildFileCompiler::containsOutputFlag(std::string const& script) {
        return containsFlag(script, isOutputFlag);
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
                    ss << "If a source file: modify the rule definition." << std::endl;
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
                    << " already owned by the rule at line " << producer->ruleLineNr()
                    << " in buildfile " << producer->buildFile()->absolutePath() << std::endl;
                throw std::runtime_error(ss.str());
            }
            if (outputNode->state() == Node::State::Deleted) outputNode->setState(Node::State::Dirty);
        } else {
            outputNode = std::make_shared<GeneratedFileNode>(_context, outputPath, cmdNode);
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
        std::vector<std::shared_ptr<Node>> const& cmdInputs,
        std::size_t defaultInputOffset
    ) const {
        std::filesystem::path outputPath = output.path;
        if (defaultInputOffset != -1) {
            static std::vector<std::shared_ptr<GeneratedFileNode>> emptyOutputs;
            static std::vector<std::shared_ptr<Node>> emptyOrderOnlyInputs;
            outputPath = compilePercentageFlags(
                _buildFile, output, _baseDir.get(), outputPath.string(),
                cmdInputs, defaultInputOffset, emptyOrderOnlyInputs,
                emptyOutputs, false);
        }
        auto base = _baseDir;
        std::filesystem::path pattern = outputPath;
        Globber::optimize(base, pattern);
        std::filesystem::path symOutputPath = base->name() / pattern;
        return symOutputPath;
    }

    std::vector<std::filesystem::path> BuildFileCompiler::compileOutputPaths(
        BuildFile::Outputs const& outputs,
        std::vector<std::shared_ptr<Node>> const& cmdInputs
    ) const {
        std::vector<std::filesystem::path> outputPaths;
        for (auto const& output : outputs.outputs) {
            if (!output.ignore) {
                if (cmdInputs.empty()) {
                    outputPaths.push_back(compileOutputPath(output, cmdInputs, -1));
                }
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
        BuildFile::Rule const& rule,
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
        } else if (_commands.contains(cmdNode->name())) {
            std::stringstream ss;
            ss << "In rule at line " << rule.line << " in buildfile " << _buildFile.string() << ":" << std::endl;
            ss << "Output file " << outputPaths[0].string()
                << " already owned by the rule at line " << cmdNode->ruleLineNr()
                << " in buildfile " << cmdNode->buildFile()->name() << std::endl;
            throw std::runtime_error(ss.str());
        }
        _commands.insert({ cmdNode->name(), cmdNode });

        return cmdNode;
    }

    void BuildFileCompiler::compileCommand(
        BuildFile::Rule const& rule,
        std::vector<std::shared_ptr<Node>> const& cmdInputs,
        std::vector<std::shared_ptr<Node>> const& orderOnlyInputs
    ) {
        std::vector<std::filesystem::path> outputPaths = compileOutputPaths(rule.outputs, cmdInputs);
        std::vector<std::filesystem::path> ignoredOutputs = compileIgnoredOutputs(rule.outputs);
        if (outputPaths.empty()) assertHasNoOutputFlag(rule.script.line, rule.script.script);

        std::shared_ptr<CommandNode> cmdNode = createCommand(rule, outputPaths);
        _ruleLineNrs[cmdNode] = rule.line;
        std::vector<std::shared_ptr<GeneratedFileNode>> outputs = createGeneratedFileNodes(rule, cmdNode, outputPaths);
        // Compile the script to check for semantic errors
        std::string notused = compileScript(_buildFile, rule.script, _baseDir.get(), cmdInputs, orderOnlyInputs, outputs);
        cmdNode->buildFile(findBuildFileNode(_context, _buildFile));
        cmdNode->ruleLineNr(rule.line);
        cmdNode->workingDirectory(_baseDir);
        cmdNode->cmdInputs(cmdInputs);
        cmdNode->orderOnlyInputs(orderOnlyInputs);
        // Note that the not-compile script must be used because the content of
        // input groupNodes must be expanded at cmdNode execution time 
        cmdNode->script(rule.script.script);
        cmdNode->outputs(outputs);
        cmdNode->ignoreOutputs(ignoredOutputs);
        for (auto const& groupPath : rule.outputGroups) {
            compileOutputGroup(rule, groupPath, outputs);
        }
        for (auto const& binPath : rule.bins) {
            compileOutputBin(rule, binPath, outputs);
        }
    }

    void BuildFileCompiler::compileOutputGroup(
        BuildFile::Rule const& rule,
        std::filesystem::path const& groupPath,
        std::vector<std::shared_ptr<GeneratedFileNode>> const& outputs
    ) {
        std::shared_ptr<GroupNode> groupNode = compileGroupNode(rule, groupPath);
        if (!_outputGroups.contains(groupNode->name())) {
            _outputGroups.insert({ groupNode->name(), groupNode });
        }
        std::vector<std::shared_ptr<Node>> group = groupNode->group();
        for (auto const& n : outputs) {
            group.push_back(n);
        }
        groupNode->group(group);
    }

    void BuildFileCompiler::compileOutputBin(
        BuildFile::Rule const& rule,
        std::filesystem::path const& binPath,
        std::vector<std::shared_ptr<GeneratedFileNode>> const& outputs
    ) {
        std::vector<std::shared_ptr<GeneratedFileNode>>& binContent = compileBin(rule, binPath);
        for (auto const& n : outputs) {
            binContent.push_back(n);
        }
    }

    void BuildFileCompiler::assertHasNoCmdInputFlag(std::size_t line, std::string const& str) const {
        if (containsFlag(str, isCmdInputFlag)) {
            std::stringstream ss;
            ss << "At line " << line << " in buildfile " << _buildFile.string() << ":" << std::endl;
            ss << "No cmd input files while '" << str << "' contains percentage flag that operates on cmd input." << std::endl;
            throw std::runtime_error(ss.str());
        }
    }

    void BuildFileCompiler::assertHasNoOrderOnlyInputFlag(std::size_t line, std::string const& str) const {
        if (containsFlag(str, isOrderOnlyInputFlag)) {
            std::stringstream ss;
            ss << "At line " << line << " in buildfile " << _buildFile.string() << ":" << std::endl;
            ss << "No order-only input files while '" << str << "' contains percentage flag that operates on order-only input." << std::endl;
            throw std::runtime_error(ss.str());
        }
    }

    void BuildFileCompiler::assertHasNoOutputFlag(std::size_t line, std::string const& str) const {
        if (containsFlag(str, isOutputFlag)) {
            std::stringstream ss;
            ss << "At line " << line << " in buildfile " << _buildFile.string() << ":" << std::endl;
            ss << "No output files while '" << str << "' contains percentage flag that operates on output." << std::endl;
            throw std::runtime_error(ss.str());
        }
    }

    bool BuildFileCompiler::isLiteralScript(std::string const& script) {
        return
            !containsFlag(script, isCmdInputFlag)
            && !containsFlag(script, isOutputFlag)
            && !containsFlag(script, isOrderOnlyInputFlag);
    }

    void BuildFileCompiler::compileRule(BuildFile::Rule const& rule) {
        std::vector<std::shared_ptr<Node>> cmdInputs = compileInputs(rule.cmdInputs);
        std::vector<std::shared_ptr<Node>> orderOnlyInputs = compileOrderOnlyInputs(rule.orderOnlyInputs);
        if (cmdInputs.empty()) assertHasNoCmdInputFlag(rule.script.line, rule.script.script);
        if (orderOnlyInputs.empty()) assertHasNoOrderOnlyInputFlag(rule.script.line, rule.script.script);
        if (rule.forEach) {
            for (auto const& cmdInput : cmdInputs) {
                std::vector<std::shared_ptr<Node>> cmdInputs_({ cmdInput });
                compileCommand(rule, cmdInputs_, orderOnlyInputs);
            }
        } else {
            compileCommand(rule, cmdInputs, orderOnlyInputs);
        }
    }  
}
