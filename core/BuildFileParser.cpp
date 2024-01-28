#include "BuildFileParser.h"
#include "BuildFileTokenizer.h"
#include "BuildFile.h"
#include "BuildFileTokenSpecs.h" // defines variable tokenSpecs
#include "Glob.h"

#include <sstream>
#include <fstream>
#include <regex>

namespace {
    using namespace YAM;

    std::string readFile(std::filesystem::path const& path) {
        std::ifstream file(path);
        std::stringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

    ITokenSpec const* whiteSpace(BuildFileTokenSpecs::whiteSpace());
    ITokenSpec const* comment1(BuildFileTokenSpecs::comment1());
    ITokenSpec const* commentN(BuildFileTokenSpecs::commentN());
    ITokenSpec const* depBuildFile(BuildFileTokenSpecs::depBuildFile());
    ITokenSpec const* depGlob(BuildFileTokenSpecs::depGlob());
    ITokenSpec const* rule(BuildFileTokenSpecs::rule());
    ITokenSpec const* foreach(BuildFileTokenSpecs::foreach());
    ITokenSpec const* ignore(BuildFileTokenSpecs::ignore());
    ITokenSpec const* curlyOpen(BuildFileTokenSpecs::curlyOpen());
    ITokenSpec const* curlyClose(BuildFileTokenSpecs::curlyClose());
    ITokenSpec const* cmdStart(BuildFileTokenSpecs::cmdStart());
    ITokenSpec const* cmdEnd(BuildFileTokenSpecs::cmdEnd());
    ITokenSpec const* script(BuildFileTokenSpecs::script());
    ITokenSpec const* vertical(BuildFileTokenSpecs::vertical());
    ITokenSpec const* glob(BuildFileTokenSpecs::glob());
}

// Conventions:
// A parse* function parses optional content.
// An eat* function parses mandatory content.
// A call to an eat* function is always preceeded by a lookAhead call.
// Only an eat* function can raise a syntax error.
//
namespace YAM {

    BuildFileParser::BuildFileParser(std::filesystem::path const& buildFilePath)
        : BuildFileParser(readFile(buildFilePath), buildFilePath)
    {}

    BuildFileParser::BuildFileParser(
        std::string const& buildFileContent,
        std::filesystem::path const& buildFilePath)
        : _buildFilePath(buildFilePath)
        , _tokenizer(buildFilePath, buildFileContent)
        , _file(parseBuildFile())
    {}

    BuildFileParser::BuildFileParser(
        std::string&& buildFileContent,
        std::filesystem::path const& buildFilePath)
        : _buildFilePath(buildFilePath)
        , _tokenizer(buildFilePath, buildFileContent)
        , _file(parseBuildFile())
    {}


    void BuildFileParser::lookAhead(std::vector<ITokenSpec const*> const& specs) {
        _tokenizer.skip({ whiteSpace, comment1, commentN });
        _lookAhead = _tokenizer.readNextToken(specs);
    }

    Token BuildFileParser::eat(ITokenSpec const* toEat ) {
        if (_lookAhead.spec != toEat) {
            syntaxError();
        }
        return _lookAhead;
    }

    void BuildFileParser::syntaxError() {
        std::stringstream ss;
        ss
            << "Unexpected token at line " << _tokenizer.line()
            << ", column " << _tokenizer.column()
            << " in file " << _tokenizer.filePath().string()
            << std::endl;
        throw std::runtime_error(ss.str());
    }

    std::shared_ptr<BuildFile::File> BuildFileParser::parseBuildFile() {
        auto file = std::make_shared<BuildFile::File>();
        file->buildFile = _buildFilePath;
        file->line = _tokenizer.tokenStartLine();
        file->column = _tokenizer.tokenStartColumn();
        parseDeps(file->deps);
        lookAhead({ rule });
        while (_lookAhead.spec == rule) {
             file->variablesAndRules.push_back(eatRule());
             lookAhead({ rule });
         }
         return file;
    }

    void BuildFileParser::parseDeps(BuildFile::Deps& deps) {
        lookAhead({ depBuildFile, depGlob });
        if (_lookAhead.spec == depBuildFile || _lookAhead.spec == depGlob) {
            deps.line = _tokenizer.tokenStartLine();
            deps.column = _tokenizer.tokenStartColumn();
            while (_lookAhead.spec == depBuildFile || _lookAhead.spec == depGlob) {
                if (_lookAhead.spec == depBuildFile) {
                    eatDepBuildFile(deps);
                } else if (_lookAhead.spec == depGlob) {
                    eatDepGlob(deps);
                }
                lookAhead({ depBuildFile, depGlob });
            }
        }
    }

    void BuildFileParser::eatDepBuildFile(BuildFile::Deps& deps) {
        Token t = eat(depBuildFile);
        lookAhead({ glob });
        std::filesystem::path path = eatPath();
        deps.depBuildFiles.push_back(path);
    }

    void BuildFileParser::eatDepGlob(BuildFile::Deps& deps) {
        Token t = eat(depGlob);
        lookAhead({ glob });
        std::filesystem::path path = eatGlob();
        deps.depGlobs.push_back(path);
    }

    std::shared_ptr<BuildFile::Rule> BuildFileParser::eatRule() {
        eat(rule);
        auto rulePtr = std::make_shared<BuildFile::Rule>();
        rulePtr->line = _tokenizer.tokenStartLine();
        rulePtr->column = _tokenizer.tokenStartColumn();

        lookAhead({ foreach });
        rulePtr->forEach = _lookAhead.spec == foreach;

        parseInputs(rulePtr->cmdInputs);
        parseOrderOnlyInputs(rulePtr->orderOnlyInputs);

        lookAhead({ script });
        eatScript(rulePtr->script);
        
        BuildFile::Outputs outputs;
        parseOutputs(outputs);
        for (auto const& output : outputs.outputs) {
            if (output.pathType == BuildFile::PathType::Group) {
                rulePtr->outputGroups.push_back(output.path);
            } else if (output.pathType == BuildFile::PathType::Bin) {
                rulePtr->bins.push_back(output.path);
            } else if (output.pathType == BuildFile::PathType::Path) {
                rulePtr->outputs.outputs.push_back(output);
            } else if (output.pathType == BuildFile::PathType::Glob) {
                // in case of output.ignore
                rulePtr->outputs.outputs.push_back(output);
            }
        }
        return rulePtr;
    }

    void BuildFileParser::parseInputs(BuildFile::Inputs& inputs) {
        lookAhead({ glob });
        if (_lookAhead.spec == glob) {
            inputs.line = _tokenizer.tokenStartLine();
            inputs.column = _tokenizer.tokenStartColumn();
            while (
                _lookAhead.spec == glob
                || _lookAhead.spec == ignore
            ) {
                BuildFile::Input in;
                eatInput(in);
                inputs.inputs.push_back(in);
                lookAhead({ glob, ignore });
            }
        }
    }

    void BuildFileParser::eatInput(BuildFile::Input& input) {
        input.line = _tokenizer.tokenStartLine();
        input.column = _tokenizer.tokenStartColumn();
        input.exclude = false;
        if (_lookAhead.spec == ignore) {
            input.exclude = true;
            lookAhead({ glob });
        }
        if (_lookAhead.spec == glob) {
            if (_lookAhead.type == "group") {
                input.pathType = BuildFile::PathType::Group;
                input.path = eatPath();
            } else if (_lookAhead.type == "bin") {
                input.pathType = BuildFile::PathType::Bin;
                input.path = eatPath();
            } else if (_lookAhead.type == "glob") {
                input.pathType = BuildFile::PathType::Glob;
                input.path = eatGlob();
            } else if (_lookAhead.type == "path") {
                input.pathType = BuildFile::PathType::Path;
                input.path = eatPath();
            } else if (_lookAhead.type == "no_endquote") {
                std::stringstream ss;
                    ss << "Missing endquote on input path"
                    << " at line " << _tokenizer.tokenStartLine()
                    << ", from column " << _tokenizer.tokenStartColumn()
                    << " to " << _tokenizer.tokenEndColumn()
                    << " in file " << _tokenizer.filePath().string()
                    << std::endl;
                    throw std::runtime_error(ss.str());
            } else {
                syntaxError();
            }
        } else {
            syntaxError();
        }
    }

    void BuildFileParser::parseOrderOnlyInputs(BuildFile::Inputs& inputs) {
        lookAhead({ vertical });
        if (_lookAhead.spec == vertical) {
            inputs.line = _tokenizer.tokenStartLine();
            inputs.column = _tokenizer.tokenStartColumn();
            parseInputs(inputs);
        }
    }

    void BuildFileParser::eatScript(BuildFile::Script& scriptNode) {
        scriptNode.script = eat(script).value;
        scriptNode.line = _tokenizer.tokenStartLine();
        scriptNode.column = _tokenizer.tokenStartColumn();
    }

    void BuildFileParser::parseOutputs(BuildFile::Outputs& outputs) {
        lookAhead({ glob });
        if (_lookAhead.spec == glob) {
            outputs.line = _tokenizer.tokenStartLine();
            outputs.column = _tokenizer.tokenStartColumn();
            while (_lookAhead.spec == glob || _lookAhead.spec == ignore) {
                BuildFile::Output out;
                eatOutput(out);
                outputs.outputs.push_back(out);
                lookAhead({ glob, ignore });
            }
        }
    }
    void BuildFileParser::eatOutput(BuildFile::Output& output) {
        output.line = _tokenizer.tokenStartLine();
        output.column = _tokenizer.tokenStartColumn();
        output.ignore = false;
        if (_lookAhead.spec == ignore) {
            output.ignore = true;
            lookAhead({ glob });
        }
        if (_lookAhead.spec == glob) {
            if (_lookAhead.type == "group") {
                output.pathType = BuildFile::PathType::Group;
                output.path = eatPath();
            } else if (_lookAhead.type == "bin") {
                output.pathType = BuildFile::PathType::Bin;
                output.path = eatPath();
            } else if (_lookAhead.type == "path") {
                output.pathType = BuildFile::PathType::Path;
                output.path = eatPath();
            } else if (_lookAhead.type == "glob") {
                if (output.ignore) {
                    output.pathType = BuildFile::PathType::Glob;
                    output.path = eatGlob();
                } else {
                    eatPath(); // will throw glob-not-allowed
                }
            } else if (_lookAhead.type == "no_endquote") {
                std::stringstream ss;
                ss << "Missing endquote on output path"
                    << " at line " << _tokenizer.tokenStartLine()
                    << ", from column " << _tokenizer.tokenStartColumn()
                    << " to " << _tokenizer.tokenEndColumn()
                    << " in file " << _tokenizer.filePath().string()
                    << std::endl;
                throw std::runtime_error(ss.str());
            } else {
                syntaxError();
            }
        } else {
            syntaxError();
        }
    }

    Token BuildFileParser::eatGlobToken() {
        Token token = eat(glob);
        std::filesystem::path path(token.value);
        if (path.is_absolute()) {
            std::stringstream ss;
            ss
                << "Illegal use of absolute path '" << path.string() << "'"
                << " at line " << _tokenizer.tokenStartLine()
                << ", from column " << _tokenizer.tokenStartColumn()
                << " to " << _tokenizer.tokenEndColumn()
                << " in file " << _tokenizer.filePath().string()
                << std::endl;
            throw std::runtime_error(ss.str());
        }
        return token;
    }

    std::filesystem::path BuildFileParser::eatGlob() {
        return eatGlobToken().value;
    }

    std::filesystem::path BuildFileParser::eatPath() {
        Token token = eatGlobToken();
        if (token.type == "glob") {
            std::stringstream ss;
            ss
                << "Illegal use of glob characters in path '" << token.value << "'"
                << " at line " << _tokenizer.tokenStartLine()
                << ", from column " << _tokenizer.tokenStartColumn()
                << " to " << _tokenizer.tokenEndColumn()
                << " in file " << _tokenizer.filePath().string()
                << std::endl;
            throw std::runtime_error(ss.str());
        }
        return token.value;
    }
}