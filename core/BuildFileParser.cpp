#include "BuildFileParser.h"
#include "BuildFileTokenizer.h"
#include "BuildFile.h"
#include "buildFileTokenSpecs.h" // defines variable tokenSpecs
#include "Glob.h"

#include <sstream>
#include <fstream>
#include <regex>

namespace {
    using namespace YAM;

    std::string readFile(std::filesystem::path const& path) {
        std::ifstream file(path.string().c_str());
        std::stringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

    TokenSpec const& findTokenSpec(std::string tokenType) {
        for (auto const& t : tokenSpecs) {
            if (t.type == tokenType) return t;
        }
        throw std::runtime_error("unknown token type " + tokenType);
    }
}

// TODO: error reporting is poor due to BuildFileTokenizer approach with
// too complex token. In particular the script token is too coarse
// causing confusing 'unexpected token errors'.
// Better errors can be generated when BuildFileParser reads |>, then consumes
// characters until next |>.
namespace YAM {

    BuildFileParser::BuildFileParser(std::filesystem::path const& buildFilePath)
        : BuildFileParser(readFile(buildFilePath), buildFilePath)
    {}

    BuildFileParser::BuildFileParser(
        std::string const& buildFileContent,
        std::filesystem::path const& buildFilePath)
        : _buildFilePath(buildFilePath)
        , _tokenizer(buildFilePath, buildFileContent, tokenSpecs)
        , _file(parseBuildFile())
    {}

    BuildFileParser::BuildFileParser(
        std::string&& buildFileContent,
        std::filesystem::path const& buildFilePath)
        : _buildFilePath(buildFilePath)
        , _tokenizer(buildFilePath, buildFileContent, tokenSpecs)
        , _file(parseBuildFile())
    {}


    Token BuildFileParser::eat(std::string const& tokenType) {
        Token eaten = _lookAhead;
        if (eaten.type == "") syntaxError(tokenType);
        if (eaten.type != tokenType) syntaxError(tokenType);
        _tokenizer.readNextToken(_lookAhead);
        return eaten;
    }

    void BuildFileParser::syntaxError(std::string const& tokenType) {
        TokenSpec const& expectedToken = findTokenSpec(tokenType);
        std::stringstream ss;
        ss
            << "Unexpected token: " << _lookAhead.type << ", "
            << "expected token: " << tokenType << " must match regex " << expectedToken.pattern
            << std::endl
            << "At line " << _tokenizer.tokenStartLine()
            << ", from column " << _tokenizer.tokenStartColumn()
            << " to " << _tokenizer.tokenEndColumn()
            << std::endl;
        throw std::runtime_error(ss.str());
    }

    std::shared_ptr<BuildFile::File> BuildFileParser::parseBuildFile() {
        auto file = std::make_shared<BuildFile::File>();
        _tokenizer.readNextToken(_lookAhead);
        file->buildFile = _buildFilePath;
        file->line = _tokenizer.tokenStartLine();
        file->column = _tokenizer.tokenStartColumn();
        if (_lookAhead.type == "depBuildFile" || _lookAhead.type == "depGlob") {
            parseDeps(file->deps);
        }
        while (_lookAhead.type != "eos") {
            if (_lookAhead.type == "rule") {
                file->variablesAndRules.push_back(parseRule());
            } else {
                eat(_lookAhead.type);
            }
        }
        return file;
    }

    void BuildFileParser::parseDeps(BuildFile::Deps& deps) {
        deps.line = _tokenizer.tokenStartLine();
        deps.column = _tokenizer.tokenStartColumn();
        while (
            _lookAhead.type == "depBuildFile"
            || _lookAhead.type == "depGlob"
        ) {
            if (_lookAhead.type == "depBuildFile") {
                eat("depBuildFile");
                std::filesystem::path path;
                parsePath(path);
                deps.depBuildFiles.push_back(path);
            } else if (_lookAhead.type == "depGlob") {
                eat("depGlob");
                std::filesystem::path glob;
                parseGlob(glob);
                deps.depGlobs.push_back(glob);
            }
        }
    }

    std::shared_ptr<BuildFile::Rule> BuildFileParser::parseRule() {
        eat("rule"); // Return value not needed
        auto rule = std::make_shared<BuildFile::Rule>();
        rule->line = _tokenizer.tokenStartLine();
        rule->column = _tokenizer.tokenStartColumn();
        if (_lookAhead.type == "foreach") {
            rule->forEach = true;
            eat("foreach");
        }
        parseInputs(rule->cmdInputs);
        parseScript(rule->script);
        parseOutputs(rule->outputs);
        return rule;
    }

    void BuildFileParser::parseInputs(BuildFile::Inputs& inputs) {
        inputs.line = _tokenizer.tokenStartLine();
        inputs.column = _tokenizer.tokenStartColumn();
        while (_lookAhead.type != "script") {
            BuildFile::Input in;
            parseInput(in);
            inputs.inputs.push_back(in);
        }
    }

    void BuildFileParser::parseInput(BuildFile::Input& input) {
        input.line = _tokenizer.tokenStartLine();
        input.column = _tokenizer.tokenStartColumn();
        input.exclude = _lookAhead.type == "not";
        if (input.exclude) eat("not");
        parseGlob(input.pathPattern);
    }

    void BuildFileParser::parseScript(BuildFile::Script& script) {
        script.line = _tokenizer.tokenStartLine();
        script.column = _tokenizer.tokenStartColumn();
        Token token = eat("script");
        script.script = token.value;
    }

    void BuildFileParser::parseOutputs(BuildFile::Outputs& outputs) {
        outputs.line = _tokenizer.tokenStartLine();
        outputs.column = _tokenizer.tokenStartColumn();
        while (_lookAhead.type == "glob") {
            BuildFile::Output out;
            parseOutput(out);
            outputs.outputs.push_back(out);
        }
    }
    void BuildFileParser::parseOutput(BuildFile::Output& output) {
        output.line = _tokenizer.tokenStartLine();
        output.column = _tokenizer.tokenStartColumn();
        output.ignore = _lookAhead.type == "not";
        parsePath(output.path);
    }

    void BuildFileParser::parseGlob(std::filesystem::path& glob) {
        Token token = eat("glob");
        glob = token.value;
    }

    void BuildFileParser::parsePath(std::filesystem::path& path) {
        if (_lookAhead.type == "glob") {
            path = _lookAhead.value;
            if (Glob::isGlob(_lookAhead.value)) {
                std::stringstream ss;
                ss
                    << "Path '" << _lookAhead.value << "' is not allowed to contain glob special characters"
                    << std::endl
                    << "At line " << _tokenizer.tokenStartLine()
                    << ", from column " << _tokenizer.tokenStartColumn()
                    << " to " << _tokenizer.tokenEndColumn()
                    << std::endl;
                throw std::runtime_error(ss.str());
            } else {
                eat("glob");
            }
        }
    }
}