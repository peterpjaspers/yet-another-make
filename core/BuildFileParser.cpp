#include "BuildFileParser.h"
#include "BuildFileTokenizer.h"
#include "BuildFile.h"
#include "buildFileTokenSpecs.h" // defines variable tokenSpecs
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
}

// TODO: error reporting is poor due to BuildFileTokenizer approach with
// too complex token. In particular the script token is too coarse
// causing confusing 'unexpected token errors'.
// Better errors can be generated when BuildFileParser reads |>, then consumes
// characters until next |>.
namespace YAM {

    BuildFileParser::BuildFileParser(std::filesystem::path const& buildFilePath)
        : BuildFileParser(readFile(buildFilePath))
    {}

    BuildFileParser::BuildFileParser(std::string const& buildFileContent)
        : _tokenizer(buildFileContent, tokenSpecs)
        , _file(parseBuildFile())
    {}

    BuildFileParser::BuildFileParser(std::string && buildFileContent)
        :  _tokenizer(buildFileContent, tokenSpecs)
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
        std::stringstream ss;
        ss
            << "Unexpected token: " << _lookAhead.type << ", "
            << "expected token: " << tokenType 
            << std::endl
            << "At line " << _tokenizer.tokenStartLine()
            << ", from column " << _tokenizer.tokenStartColumn()
            << " to " << _tokenizer.tokenEndColumn()
            << std::endl;
        throw std::runtime_error(ss.str());
    }

    std::shared_ptr<BuildFile::File> BuildFileParser::parseBuildFile() {
        _tokenizer.readNextToken(_lookAhead);
        auto file = std::make_shared<BuildFile::File>();
        while (_lookAhead.type != "eos") {
            if (_lookAhead.type == "rule") {
                file->variablesAndRules.push_back(parseRule());
            } // else if variable, etc
        }
        return file;
    }

    std::shared_ptr<BuildFile::Rule> BuildFileParser::parseRule() {
        eat("rule"); // Return value not needed
        auto rule = std::make_shared<BuildFile::Rule>();
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
        while (_lookAhead.type != "script") {
            BuildFile::Input in;
            parseInput(in);
            inputs.inputs.push_back(in);
        }
    }

    void BuildFileParser::parseInput(BuildFile::Input& input) {
        input.exclude = _lookAhead.type == "not";
        if (input.exclude) eat("not");
        auto pathToken = eat("glob");
        input.pathPattern = pathToken.value;
    }

    void BuildFileParser::parseScript(BuildFile::Script& script) {
        Token token = eat("script");
        script.script = token.value;
    }

    void BuildFileParser::parseOutputs(BuildFile::Outputs& outputs) {
        while (_lookAhead.type != "eos") {
            BuildFile::Output out;
            parseOutput(out);
            outputs.outputs.push_back(out);
        }
    }
    void BuildFileParser::parseOutput(BuildFile::Output& output) {
        Token token = eat("glob");
        output.path = token.value;
    }
}