#include "BuildFileCompiler.h"
#include "PercentageFlagsCompiler.h"
#include "ExecutionContext.h"
#include "NodeSet.h"
#include "FileNode.h"
#include "SourceFileNode.h"
#include "GeneratedFileNode.h"
#include "DirectoryNode.h"
#include "CommandNode.h"
#include "ForEachNode.h"
#include "FileRepositoryNode.h"
#include "Globber.h"
#include "Glob.h"
#include "GlobNode.h"
#include "GroupNode.h"
#include "AcyclicTrail.h"

#include <sstream>
#include <ctype.h>
#include <algorithm>

namespace
{
    using namespace YAM;

    bool containsGroup(std::vector<std::shared_ptr<Node>> const& nodes) {
        for (auto const& n : nodes) {
            auto const& group = dynamic_pointer_cast<GroupNode>(n);
            if (group != nullptr) return true;
        }
        return false;
    }

    BuildFile::Output findFirstMandatoryOutput(std::vector<CommandNode::OutputFilter> const& filters) {
        BuildFile::Output output;
        output.ignore = false;
        output.pathType = BuildFile::PathType::Path;
        for (auto const& filter : filters) {
            if (
                filter._type == CommandNode::OutputFilter::Type::Output
                || filter._type == CommandNode::OutputFilter::Type::ExtraOutput
            ) {
                output.path = filter._path;
            }
        }
        return output;
    }

    std::filesystem::path uidPath(std::filesystem::path const& baseDir) {
        std::stringstream ss;
        ss << rand();
        return std::filesystem::path(baseDir / ss.str());
    }

    std::filesystem::path uniqueName(
        std::filesystem::path const& baseDir,
        NodeSet& nodes,
        std::map<std::filesystem::path, std::shared_ptr<Node>> const& newNodes
    ) {
        std::filesystem::path uid = uidPath(baseDir);
        while (
            (nodes.find(uid) != nullptr)
            && (newNodes.find(uid) != newNodes.end())
        ) {
            uid = uidPath(baseDir);
        }
        return uid;
    }

    bool containsOptionalFilter(std::vector<CommandNode::OutputFilter> const &filters) {
        for (auto const& f : filters) {
            if (f._type == CommandNode::OutputFilter::Type::Optional) return true;
        }
        return false;
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


    std::set<std::shared_ptr<Node>, Node::CompareName> contributionToGroup(
        std::shared_ptr<GroupNode> const& group,
        std::set<std::shared_ptr<Node>, Node::CompareName> const& nodes
    ) {
        std::set<std::shared_ptr<Node>, Node::CompareName> contrib;
        std::set<std::shared_ptr<Node>, Node::CompareName> const& content = group->content();
        for (auto const& node : nodes) {
            if (content.contains(node)) contrib.insert(node);
        }
        return contrib;
    }

    std::map<std::filesystem::path, std::set<std::shared_ptr<Node>, Node::CompareName>> contributionToGroups(
        std::map<std::filesystem::path, std::shared_ptr<GroupNode>> const& groups,
        std::set<std::shared_ptr<Node>, Node::CompareName> const& nodes
    ) {
        std::map<std::filesystem::path, std::set<std::shared_ptr<Node>, Node::CompareName>> contribs;
        for (auto const& pair : groups) {
            contribs.insert({ pair.first, contributionToGroup(pair.second, nodes)});
        }
        return contribs;
    }

    std::set<std::shared_ptr<Node>, Node::CompareName> toSet(
        std::map<std::filesystem::path, std::shared_ptr<CommandNode>> const& commands
    ) {
        std::set<std::shared_ptr<Node>, Node::CompareName> nodes;
        for (auto const& pair : commands) {
            nodes.insert(pair.second);
        }
        return nodes;
    }

    void removeFromGroup(
        std::shared_ptr<GroupNode> const& groupNode,
        std::set<std::shared_ptr<Node>, Node::CompareName> const& nodes
     ) {
        for (auto const& node : nodes) groupNode->remove(node);
    }

    void addToGroup(
        std::shared_ptr<GroupNode> const& groupNode,
        std::set<std::shared_ptr<Node>, Node::CompareName> const& nodes
    ) {
        for (auto const& node : nodes) groupNode->add(node);
    }

    void updateGroup(
        std::shared_ptr<GroupNode> const& groupNode,
        std::set<std::shared_ptr<Node>, Node::CompareName> const& oldContent,
        std::set<std::shared_ptr<Node>, Node::CompareName> const& newContent
    ) {
        for (auto const& node : oldContent) {
            if (!newContent.contains(node)) {
                groupNode->remove(node);
            }
        }
        for (auto const& node : newContent) {
            if (!oldContent.contains(node)) {
                groupNode->add(node);
            }
        }
    }

}

namespace YAM 
{
    BuildFileCompiler::BuildFileCompiler(
            ExecutionContext* context,
            std::shared_ptr<DirectoryNode> const& baseDir,
            BuildFile::File const& buildFile,
            std::map<std::filesystem::path, std::shared_ptr<CommandNode>> const& commands,
            std::map<std::filesystem::path, std::shared_ptr<ForEachNode>> const& forEachNodes,
            std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> const& mandatoryOutputs,
            std::map<std::filesystem::path, std::shared_ptr<GroupNode>> const& outputGroups,
            std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> const& allowedInputs,
            std::filesystem::path const& globNameSpace)
        : _context(context)
        , _baseDir(baseDir)
        , _buildFile(buildFile.buildFile)
        , _oldCommands(commands)
        , _oldForEachNodes(forEachNodes)
        , _oldMandatoryOutputs(mandatoryOutputs)
        , _oldOutputGroups(outputGroups)
        , _oldOutputGroupsContent(contributionToGroups(_oldOutputGroups, toSet(_oldCommands)))
        , _allowedInputs(allowedInputs)
        , _globNameSpace(globNameSpace)
    {
        for (auto const& glob : buildFile.deps.depGlobs) {
            compileGlob(glob);
        }
        for (auto const& varOrRule : buildFile.variablesAndRules) {
            auto rule = dynamic_cast<BuildFile::Rule*>(varOrRule.get());
            if (rule != nullptr) compileRule(*rule);
        }
        updateOutputGroups();
    }

    void BuildFileCompiler::updateOutputGroups() {
        static Node::CompareName cmp;
        for (auto const& pair : _outputGroupsContent) {
            auto const &groupPath = pair.first;
            auto const &content = pair.second;
            auto groupNode = compileOutputGroup(groupPath);
            auto oldIt = _oldOutputGroupsContent.find(groupPath);
            if (oldIt == _oldOutputGroupsContent.end()) {
                addToGroup(groupNode, content);
            } else {
                auto const &oldContent = oldIt->second;
                updateGroup(groupNode, oldContent, content);
            }
        }
        for (auto const& pair : _oldOutputGroups) {
            auto const& groupPath = pair.first;
            auto groupNode = pair.second;
            if (!_outputGroups.contains(groupPath)) {
                auto oldIt = _oldOutputGroupsContent.find(groupPath);
                removeFromGroup(groupNode, oldIt->second);
            }
        }
    }

    std::shared_ptr<GroupNode> BuildFileCompiler::compileGroupNode(
        std::filesystem::path const& groupName
    ) {
        auto optimizedBaseDir = _baseDir;
        auto optimizedPattern = groupName;
        Globber::optimize(_context, optimizedBaseDir, optimizedPattern);
        std::filesystem::path groupPath(optimizedBaseDir->name() / optimizedPattern);

        std::shared_ptr<GroupNode> groupNode;
        auto it = _outputGroups.find(groupPath);
        if (it != _outputGroups.end()) groupNode = it->second;
        if (groupNode == nullptr) {
            auto it = _newOutputGroups.find(groupPath);
            if (it != _newOutputGroups.end()) groupNode = it->second;
        }
        if (groupNode == nullptr) {
            auto node = _context->nodes().find(groupPath);
            groupNode = dynamic_pointer_cast<GroupNode>(node);
            if (node != nullptr && groupNode == nullptr) {
                std::stringstream ss;
                ss << "In buildfile " << _buildFile.string() << ":" << std::endl;
                ss << "The group name " << groupName.string()
                    << " is already in use by a node that is not a group," << std::endl;
                throw std::runtime_error(ss.str());
            }
            if (groupNode == nullptr) {
                groupNode = std::make_shared<GroupNode>(_context, groupPath);
                _newOutputGroups.insert({ groupNode->name(), groupNode});
            }
        }
        return groupNode;
    }

    std::vector<std::shared_ptr<GeneratedFileNode>>& BuildFileCompiler::compileBin(
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
        auto groupNode = compileGroupNode(input.path);
        _context->nodes().addIfAbsent(groupNode);
        return groupNode;
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
        BuildFile::Input const& input
    ) {
        auto optimizedBaseDir = _baseDir;
        auto optimizedPattern = input.path;
        Globber::optimize(_context, optimizedBaseDir, optimizedPattern);
        std::filesystem::path inputPath(optimizedBaseDir->name() / optimizedPattern);
        auto fileNode = dynamic_pointer_cast<FileNode>(_context->nodes().find(inputPath));
        if (fileNode == nullptr) {
            auto it = _mandatoryOutputs.find(inputPath);
            if (it != _mandatoryOutputs.end()) fileNode = dynamic_pointer_cast<FileNode>(it->second);
        }
        auto it = _oldMandatoryOutputs.find(inputPath);
        if (fileNode == nullptr || it != _oldMandatoryOutputs.end()) {
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
        auto genFile = dynamic_pointer_cast<GeneratedFileNode>(fileNode);
        if (genFile != nullptr) {
            if (_allowedInputs.find(genFile->name()) == _allowedInputs.end()) {
                auto cmd = genFile->producer();
                auto cmdBuildFile = cmd->buildFile();
                std::string cmdBuildFileName = (cmdBuildFile == nullptr) ? "unknown" : cmdBuildFile->name().string(); 
                std::stringstream ss;
                ss 
                    << "The rule at line " << input.line << " in buildfile " << _buildFile.string()
                    << " has an illegal reference to generated file " << genFile->name() << "." << std::endl;
                ss 
                    << "This file is defined as output of the rule at line " << cmd->ruleLineNr()
                    <<" in buildfile " << cmdBuildFileName << "." << std::endl;
                ss << "Fix: declare " << cmdBuildFileName << " as buildfile dependency in buildfile " << _buildFile.string();
                ss << "." << std::endl;
                throw std::runtime_error(ss.str());
            }
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
            // Match source files
            for (auto const& match : globNode->matches()) {
                fileNode = dynamic_pointer_cast<FileNode>(match); 
                if (fileNode == nullptr) {
                    std::stringstream ss;
                    ss << "In rule input section at line " << input.line << " in buildfile " << _buildFile.string() << ":" << std::endl;
                    ss << "Glob " << input.path << " matches " << match->name() << " which is not a file." << std::endl;
                    ss << "Make sure that an input glob only matches files" << std::endl;
                    throw std::runtime_error(ss.str());
                }
                inputNodes.push_back(fileNode);
            }
            // Match generated files
            if (!_allowedInputs.empty()) {
                std::filesystem::path pattern = globNode->baseDirectory()->name() / globNode->pattern();
                Glob glob(pattern);
                for (auto const& pair : _allowedInputs) {
                    if (glob.matches(pair.second->name())) {
                        inputNodes.push_back(pair.second);
                    }
                }
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
        Globber::optimize(_context, optimizedBaseDir, optimizedPattern);
        std::filesystem::path globName(_globNameSpace / optimizedBaseDir->name() / optimizedPattern);
        auto globNode = dynamic_pointer_cast<GlobNode>(_context->nodes().find(globName));
        if (globNode == nullptr) {
            auto it = _newInputGlobs.find(globName);
            if (it != _newInputGlobs.end()) globNode = it->second;
        }
        if (globNode == nullptr) {
            globNode = std::make_shared<GlobNode>(_context, globName);
            globNode->baseDirectory(optimizedBaseDir);
            globNode->pattern(optimizedPattern);
            globNode->initialize();
            _newInputGlobs.insert({ globNode->name(), globNode });
        }
        _inputGlobs.insert({ globNode->name(), globNode });
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

    std::shared_ptr<GeneratedFileNode> BuildFileCompiler::createGeneratedFileNode(
        BuildFile::Rule const& rule,
        std::shared_ptr<CommandNode> const& cmdNode,
        std::filesystem::path const& outputPath
    ) {
        std::shared_ptr<GeneratedFileNode> outputNode;

        auto it = _oldMandatoryOutputs.find(outputPath);
        if (it != _oldMandatoryOutputs.end()) {
            outputNode = it->second;
            _oldMandatoryOutputs.erase(it);
        } else {
            std::shared_ptr<Node> node = _context->nodes().find(outputPath);
            if (node == nullptr) {
                auto it = _newMandatoryOutputs.find(outputPath);
                if (it != _newMandatoryOutputs.end()) {
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
                } else if (outputNode->producer() != cmdNode) {
                    auto producer = outputNode->producer();
                    std::stringstream ss;
                    ss << "In rule at line " << rule.line << " in buildfile " << _buildFile.string() << ":" << std::endl;
                    ss << "Output file " << outputNode->name().string()
                        << " already owned by the rule at line " << producer->ruleLineNr()
                        << " in buildfile " << producer->buildFile()->absolutePath() << std::endl;
                    throw std::runtime_error(ss.str());
                }
            } else {
                outputNode = std::make_shared<GeneratedFileNode>(_context, outputPath, cmdNode);
                _newMandatoryOutputs.insert({ outputPath, outputNode });
            }
        }
        _mandatoryOutputs.insert({ outputNode->name(), outputNode });
        _allowedInputs.insert({ outputNode->name(), outputNode });
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
            PercentageFlagsCompiler flagsCompiler(
                _buildFile, output, _context,
                 _baseDir, cmdInputs, defaultInputOffset);
            outputPath = flagsCompiler.result();
        }
        auto base = _baseDir;
        std::filesystem::path pattern = outputPath;
        Globber::optimize(_context, base, pattern);
        std::filesystem::path symOutputPath = base->name() / pattern;
        return symOutputPath;
    }

    std::filesystem::path BuildFileCompiler::compileFirstOutputPath(
        BuildFile::Outputs const& outputs,
        std::vector<std::shared_ptr<Node>> const& cmdInputs
    ) const {
        std::filesystem::path firstOutputPath;
        BuildFile::Output firstOutput;
        firstOutput.pathType = BuildFile::PathType::Bin;
        for (auto const& output : outputs.outputs) {
            if (output.pathType == BuildFile::PathType::Bin) continue;
            if (output.pathType == BuildFile::PathType::Group) continue;
            if (output.ignore && firstOutput.pathType == BuildFile::PathType::Bin) {
                firstOutput = output;
            } else if (output.pathType == BuildFile::PathType::Path) {
                firstOutput = output;
                break;
            } else if (output.pathType == BuildFile::PathType::Glob) {
                firstOutput = output;
            }
        }
        if (!firstOutput.path.empty()) {
            bool hasFlags = PercentageFlagsCompiler::containsFlags(firstOutput.path.string());
            if (cmdInputs.empty() || !hasFlags) {
                firstOutputPath = compileOutputPath(firstOutput, cmdInputs, -1);
            } else {
                firstOutputPath = compileOutputPath(firstOutput, cmdInputs, 0); \
            }
        }
        return firstOutputPath;
    }

    std::vector<std::filesystem::path> BuildFileCompiler::compileMandatoryOutputPaths(
        BuildFile::Outputs const& outputs,
        std::vector<std::shared_ptr<Node>> const& cmdInputs
    ) const {
        std::vector<std::filesystem::path> outputPaths;
        for (auto const& output : outputs.outputs) {
            if (output.ignore) continue;
            if (output.pathType != BuildFile::PathType::Path) continue;
            bool hasFlags = PercentageFlagsCompiler::containsFlags(output.path.string());
            // When output does not contain %-flags then duplicate outputPaths will
            // be generated when there are multiple cmdInputs. 
            // E.g. if output is main.exe and cnmInputs are a.obj and b.obj then
            // output main.exe would be generated twice
            if (cmdInputs.empty() || !hasFlags) {
                outputPaths.push_back(compileOutputPath(output, cmdInputs, -1));
            } else {
                for (std::size_t i = 0; i < cmdInputs.size(); ++i) {
                    std::filesystem::path outputPath = compileOutputPath(output, cmdInputs, i);
                    outputPaths.push_back(outputPath);
                }
            }
        }
        return outputPaths;
    }

    CommandNode::OutputFilter BuildFileCompiler::compileOutputFilter(
        BuildFile::Output const& output,
        std::vector<std::shared_ptr<Node>> const& cmdInputs,
        std::size_t defaultInputOffset
    ) const {
        std::filesystem::path outputPath = output.path;
        if (defaultInputOffset != -1) {
            PercentageFlagsCompiler flagsCompiler(
                _buildFile, output, _context,
                 _baseDir, cmdInputs, defaultInputOffset);
            outputPath = flagsCompiler.result();
        } else {
            auto baseDir = _baseDir;
            auto pattern = output.path;
            Globber::optimize(_context, baseDir, pattern);
            outputPath = baseDir->name() / pattern;
        }
        CommandNode::OutputFilter::Type filterType;
        if (output.ignore) filterType = CommandNode::OutputFilter::Type::Ignore;
        else if (output.pathType == BuildFile::PathType::Path) filterType = CommandNode::OutputFilter::Type::Output;
        else if (output.pathType == BuildFile::PathType::Glob) filterType = CommandNode::OutputFilter::Type::Optional;
        else throw std::exception("bad type");
        CommandNode::OutputFilter filter(filterType, outputPath);
        return filter;
    }

    std::vector<CommandNode::OutputFilter> BuildFileCompiler::compileOutputFilters(
        BuildFile::Outputs const& outputs,
        std::vector<std::shared_ptr<Node>> const& cmdInputs
    ) const {
        std::vector<CommandNode::OutputFilter> filters;
        for (auto const& output : outputs.outputs) {
            bool hasFlags = PercentageFlagsCompiler::containsFlags(output.path.string());
            // When output does not contain %-flags then duplicate output 
            // filters will be generated when there are multiple cmdInputs. 
            // E.g. if output==main.exe and inputs are a.obj and b.obj then two
            // filters for main.exe would be generated.
            if (cmdInputs.empty() || !hasFlags) {
                CommandNode::OutputFilter filter = compileOutputFilter(output, cmdInputs, -1);
                filters.push_back(filter);
            } else {
                for (std::size_t i = 0; i < cmdInputs.size(); ++i) {
                    CommandNode::OutputFilter filter = compileOutputFilter(output, cmdInputs, i);
                    filters.push_back(filter);
                }
            }
        }
        return filters;
    }

    std::vector<std::filesystem::path> BuildFileCompiler::compileIgnoredOutputs(
        BuildFile::Outputs const& outputs
    ) const {
        std::vector<std::filesystem::path> ignoredOutputs;
        for (auto const& output : outputs.outputs) {
            if (output.ignore) {
                auto base = _baseDir;
                std::filesystem::path pattern = output.path;
                Globber::optimize(_context, base, pattern);
                std::filesystem::path symOutputPath = base->name() / pattern;
                ignoredOutputs.push_back(symOutputPath);
            }
        }
        return ignoredOutputs;
    }

    std::shared_ptr<CommandNode> BuildFileCompiler::createCommand(
        BuildFile::Rule const& rule,
        std::filesystem::path const& firstOutputPath)
    {
        std::filesystem::path cmdName;
        if (firstOutputPath.empty()) {
            cmdName = uniqueName(_baseDir->name(), _context->nodes(), _newCommandsAndForEachNodes);
        } else {
            cmdName = firstOutputPath / "__cmd";
        }
        std::shared_ptr<Node> node = _context->nodes().find(cmdName);
        auto cmdNode = dynamic_pointer_cast<CommandNode>(node);
        if (node != nullptr && cmdNode == nullptr) {
            throw std::runtime_error("not a command node"); //TODO: better message
        } else if (cmdNode == nullptr) {
            cmdNode = std::make_shared<CommandNode>(_context, cmdName);
            _newCommandsAndForEachNodes.insert({ cmdName, cmdNode });
        }
        if (_commands.contains(cmdName) || _forEachNodes.contains(cmdName)) {
            std::stringstream ss;
            ss << "In rule at line " << rule.line << " in buildfile " << _buildFile.string() << ":" << std::endl;
            ss << "Output file " << cmdName.string()
                << " already owned by the rule at line " << cmdNode->ruleLineNr()
                << " in buildfile " << cmdNode->buildFile()->name() << std::endl;
            throw std::runtime_error(ss.str());
        }
        _commands.insert({ cmdName, cmdNode });
        return cmdNode;
    }

    void BuildFileCompiler::compileCommand(
        BuildFile::Rule const& rule,
        std::vector<std::shared_ptr<Node>> const& cmdInputs,
        std::vector<std::shared_ptr<Node>> const& orderOnlyInputs
    ) {
        std::vector<CommandNode::OutputFilter> outputFilters = compileOutputFilters(rule.outputs, cmdInputs);
        std::vector<std::filesystem::path> outputPaths = compileMandatoryOutputPaths(rule.outputs, cmdInputs);
        std::filesystem::path firstOutputPath = compileFirstOutputPath(rule.outputs, cmdInputs);
        std::shared_ptr<CommandNode> cmdNode = createCommand(rule, firstOutputPath);

        _cmdRuleLineNrs[cmdNode] = rule.line;
        cmdNode->buildFile(findBuildFileNode(_context, _buildFile));
        cmdNode->ruleLineNr(rule.line);
        cmdNode->workingDirectory(_baseDir);
        cmdNode->cmdInputs(cmdInputs);
        cmdNode->orderOnlyInputs(orderOnlyInputs);
        // Note that the not-compiled script must be used because the content of
        // input groupNodes can only be expanded at cmdNode execution time.
        cmdNode->script(rule.script.script);
        if (outputFilters != cmdNode->outputFilters()) {
            // clear filters to release ownership of optional outputs that
            // may have been converted to mandatory outputs. In that case
            // createGeneratedFileNodes would fail because mandatory output
            // would be already owned as optional output.
            cmdNode->outputFilters({}, {});
        }
        std::vector<std::shared_ptr<GeneratedFileNode>> mandatoryOutputs = createGeneratedFileNodes(rule, cmdNode, outputPaths);
        cmdNode->outputFilters(outputFilters, mandatoryOutputs);

        // Compile the script to check for syntax errors, ignore the compilation
        // result. Note that the compilation will be done again by the cmdNode
        // and that the cmdNode will pass the result to a shell for execution.
        PercentageFlagsCompiler flagsCompiler(
            _buildFile,
            rule.script,
            _baseDir,
            cmdInputs,
            orderOnlyInputs,
            mandatoryOutputs);

        for (auto const& node : mandatoryOutputs) {
            _mandatoryOutputs.insert({ node->name(), node });
        }
        for (auto const& groupPath : rule.outputGroups) {
            compileOutputGroupContent(groupPath, cmdNode);
        }
        for (auto const& binPath : rule.bins) {
            compileOutputBin(binPath, cmdNode->mandatoryOutputs());
        }
    }

    std::shared_ptr<ForEachNode> BuildFileCompiler::createForEach(
        BuildFile::Rule const& rule,
        std::filesystem::path const& firstOutputPath)
    {
        std::filesystem::path forEachName;
        if (firstOutputPath.empty()) {
            forEachName = uniqueName(_baseDir->name(), _context->nodes(), _newCommandsAndForEachNodes);
        } else {
            forEachName = firstOutputPath / "__foreach";
        }
        std::shared_ptr<Node> node = _context->nodes().find(forEachName);
        auto forEachNode = dynamic_pointer_cast<ForEachNode>(node);
        if (node != nullptr && forEachNode == nullptr) {
            throw std::runtime_error("not a foreach node"); //TODO: better message
        } else if (forEachNode == nullptr) {
            forEachNode = std::make_shared<ForEachNode>(_context, forEachName);
            _newCommandsAndForEachNodes.insert({ forEachName, forEachNode });
        }
        if (_forEachNodes.contains(forEachName) || _commands.contains(forEachName)) {
            std::stringstream ss;
            ss << "In rule at line " << rule.line << " in buildfile " << _buildFile.string() << ":" << std::endl;
            ss << "Output file " << forEachName.string()
                << " already owned by the rule at line " << forEachNode->ruleLineNr()
                << " in buildfile " << forEachNode->buildFile()->name() << std::endl;
            throw std::runtime_error(ss.str());
        }
        _forEachNodes.insert({ forEachName, forEachNode });
        return forEachNode;
    }

    void BuildFileCompiler::compileForEach(
        BuildFile::Rule const& rule,
        std::vector<std::shared_ptr<Node>> const& cmdInputs,
        std::vector<std::shared_ptr<Node>> const& orderOnlyInputs
    ) {
        std::vector<CommandNode::OutputFilter> outputFilters = compileOutputFilters(rule.outputs, cmdInputs);
        std::vector<std::filesystem::path> outputPaths = compileMandatoryOutputPaths(rule.outputs, cmdInputs);
        std::filesystem::path firstOutputPath = compileFirstOutputPath(rule.outputs, cmdInputs);

        std::shared_ptr<ForEachNode> forEachNode = createForEach(rule, firstOutputPath);
        _forEachRuleLineNrs[forEachNode] = rule.line;
        forEachNode->buildFile(findBuildFileNode(_context, _buildFile));
        forEachNode->ruleLineNr(rule.line);
        forEachNode->workingDirectory(_baseDir);
        forEachNode->cmdInputs(cmdInputs);
        forEachNode->orderOnlyInputs(orderOnlyInputs);
        forEachNode->script(rule.script.script);
        forEachNode->outputs(rule.outputs);

        for (auto const& groupPath : rule.outputGroups) {
            compileOutputGroupContent(groupPath, forEachNode);
        }
    }

    std::shared_ptr<GroupNode> BuildFileCompiler::compileOutputGroup(
        std::filesystem::path const& groupPath
    ) {
        std::shared_ptr<GroupNode> groupNode = compileGroupNode(groupPath);
        if (!_outputGroups.contains(groupNode->name())) {
            _outputGroups.insert({ groupNode->name(), groupNode });
        }
        return groupNode;
    }

    void BuildFileCompiler::compileOutputBin(
        std::filesystem::path const& binPath,
        CommandNode::OutputNodes const& outputs
    ) {
        std::vector<std::shared_ptr<GeneratedFileNode>>& binContent = compileBin(binPath);
        for (auto const& pair : outputs) binContent.push_back(pair.second);
    }

    void BuildFileCompiler::compileOutputGroupContent(
        std::filesystem::path const& groupName,
        std::shared_ptr<Node> const& cmdOrForEachNode
    ) {
        auto optimizedBaseDir = _baseDir;
        auto optimizedPattern = groupName;
        Globber::optimize(_context, optimizedBaseDir, optimizedPattern);
        std::filesystem::path groupPath(optimizedBaseDir->name() / optimizedPattern);

        auto it = _outputGroupsContent.find(groupPath);
        if (it == _outputGroupsContent.end()) {
            it = _outputGroupsContent.insert({ groupPath, std::set<std::shared_ptr<Node>, Node::CompareName>() }).first;
        }
        auto& content = it->second;
        content.insert(cmdOrForEachNode);
    }

    void BuildFileCompiler::compileRule(BuildFile::Rule const& rule) {
        std::vector<std::shared_ptr<Node>> cmdInputs = compileInputs(rule.cmdInputs);
        std::vector<std::shared_ptr<Node>> orderOnlyInputs = compileOrderOnlyInputs(rule.orderOnlyInputs);
        if (rule.forEach) {
            if (containsGroup(cmdInputs)) {
                compileForEach(rule, cmdInputs, orderOnlyInputs);
            } else {
                for (auto const& cmdInput : cmdInputs) {
                    std::vector<std::shared_ptr<Node>> cmdInputs_({ cmdInput });
                    compileCommand(rule, cmdInputs_, orderOnlyInputs);
                }
            }
        } else {
            compileCommand(rule, cmdInputs, orderOnlyInputs);
        }
    }  
}
