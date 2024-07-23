#include "PercentageFlagsCompiler.h"
#include "ExecutionContext.h"
#include "CommandNode.h"
#include "GeneratedFileNode.h"
#include "DirectoryNode.h"
#include "FileRepositoryNode.h"
#include "Globber.h"

namespace
{
    using namespace YAM;

    template<typename TNode>
    std::vector<std::filesystem::path> relativePathsOf(
        DirectoryNode const* baseDir,
        std::vector<std::shared_ptr<TNode>> const& nodes
    ) {
        std::vector<std::filesystem::path> relPaths;
        if (!nodes.empty()) {
            std::shared_ptr<FileRepositoryNode> baseRepo = baseDir->repository();
            for (auto const& node : nodes) {
                if (baseRepo->lexicallyContains(node->name())) {
                    relPaths.push_back(node->name().lexically_relative(baseDir->name()));
                } else {
                    relPaths.push_back(node->absolutePath());
                }
            }
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
                "Too large offset " << offset + 1
                << " at line " << node.line << " at column " << node.column + columnOffset
                << " in build file " << buildFile.string()
                << std::endl;
            throw std::runtime_error(ss.str());
        }
    }

    std::string compileFlag1(
        std::filesystem::path const& buildFile,
        BuildFile::Node const& node,
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
        std::size_t offset,
        std::vector<std::filesystem::path> const& filePaths,
        char flag,
        std::string& result
    ) {
        if (offset == -1) {
            std::size_t nFiles = filePaths.size();
            for (std::size_t i = 0; i < nFiles; ++i) {
                auto path = compileFlag1(buildFile, node, filePaths[i], flag);
                result.append(path);
                if (i < nFiles - 1) result.append(" ");
            }
        } else {
            std::filesystem::path filePath = filePaths[offset];
            auto path = compileFlag1(buildFile, node, filePath, flag);
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
                } else {
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
                        compileFlagN(buildFile, node, offset, cmdOutputPaths, chars[i], result);
                    } else if (chars[i] == 'i') {
                        assertOffset(buildFile, node, oidx, offset, orderOnlyInputPaths.size());
                        compileFlagN(buildFile, node, offset, orderOnlyInputPaths, chars[i], result);
                    } else {
                        assertOffset(buildFile, node, oidx, offset, cmdInputPaths.size());
                        if (offset == -1) offset = defaultCmdInputOffset;
                        compileFlagN(buildFile, node, offset, cmdInputPaths, chars[i], result);
                    }
                }
            } else {
                result.insert(result.end(), chars[i]);
            }
        }
        return result;
    }


    void assertHasNoCmdInputFlag(std::filesystem::path const& buildFile, std::size_t line, std::string const& str) {
        if (containsFlag(str, isCmdInputFlag)) {
            std::stringstream ss;
            ss << "At line " << line << " in buildfile " << buildFile.string() << ":" << std::endl;
            ss << "No cmd input files while '" << str << "' expects at least one cmd input file." << std::endl;
            throw std::runtime_error(ss.str());
        }
    }

    void assertHasNoOrderOnlyInputFlag(std::filesystem::path const& buildFile, std::size_t line, std::string const& str) {
        if (containsFlag(str, isOrderOnlyInputFlag)) {
            std::stringstream ss;
            ss << "At line " << line << " in buildfile " << buildFile.string() << ":" << std::endl;
            ss << "No order-only input files while '" << str << "' expects at least one order-only input file." << std::endl;
            throw std::runtime_error(ss.str());
        }
    }

    void assertHasNoOutputFlag(std::filesystem::path const& buildFile, std::size_t line, std::string const& str) {
        if (containsFlag(str, isOutputFlag)) {
            std::stringstream ss;
            ss << "At line " << line << " in buildfile " << buildFile.string() << ":" << std::endl;
            ss << "No output files while '" << str << "' expects at least one output file." << std::endl;
            throw std::runtime_error(ss.str());
        }
    }
}

namespace YAM
{
    PercentageFlagsCompiler::PercentageFlagsCompiler(
        std::filesystem::path const& buildFile,
        BuildFile::Output const &output,
        ExecutionContext* context,
        std::shared_ptr<DirectoryNode> const& baseDir,
        std::vector<std::shared_ptr<Node>> cmdInputs,
        std::size_t defaultInputOffset)
    {
        std::filesystem::path outputPath = output.path;
        if (defaultInputOffset != -1) {
            static std::vector<std::shared_ptr<GeneratedFileNode>> emptyOutputs;
            static std::vector<std::shared_ptr<Node>> emptyOrderOnlyInputs;
            outputPath = compilePercentageFlags(
                buildFile, output, baseDir.get(), outputPath.string(),
                cmdInputs, defaultInputOffset, emptyOrderOnlyInputs,
                emptyOutputs, false);
        }
        auto base = baseDir;
        std::filesystem::path pattern = outputPath;
        Globber::optimize(context, base, pattern);
        _result = (base->name() / pattern).string();
    }

    PercentageFlagsCompiler::PercentageFlagsCompiler(
        std::filesystem::path const& buildFile,
        BuildFile::Script const &script,
        std::shared_ptr<DirectoryNode> const& baseDir,
        std::vector<std::shared_ptr<Node>> cmdInputs,
        std::vector<std::shared_ptr<Node>> orderOnlyInputs,
        std::vector<std::shared_ptr<GeneratedFileNode>> const& outputs)
    {
        if (cmdInputs.empty()) assertHasNoCmdInputFlag(buildFile, script.line, script.script);
        if (orderOnlyInputs.empty()) assertHasNoOrderOnlyInputFlag(buildFile, script.line, script.script);
        if (outputs.empty()) assertHasNoOutputFlag(buildFile, script.line, script.script);
        _result =
            compilePercentageFlags(
                buildFile, script, baseDir.get(), script.script,
                cmdInputs, -1, orderOnlyInputs,
                outputs, true);
    }

    bool PercentageFlagsCompiler::containsFlags(std::string const& script) {
        return
            containsFlag(script, isCmdInputFlag)
            || containsFlag(script, isOutputFlag)
            || containsFlag(script, isOrderOnlyInputFlag);
    }
}
