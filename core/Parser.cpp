#include "Parser.h"
#include "Tokenizer.h"
#include "SyntaxTree.h"
#include "tokenSpecs.h" // defines variable tokenSpecs
#include <sstream>
#include <regex>

namespace {
    using namespace YAM;

    std::string readFile(std::filesystem::path const& path) {
        return std::string("");
    }
}

namespace YAM {

    Parser::Parser(std::filesystem::path const& buildFilePath)
        : Parser(readFile(buildFilePath))
    {}

    Parser::Parser(std::string const& buildFileContent)
        : _content(buildFileContent)
        , _tokenizer(_content, tokenSpecs)
        , _root(parseBuildFile())
    {}

    Parser::Parser(std::string && buildFileContent)
        : _content(std::move(buildFileContent))
        , _tokenizer(_content, tokenSpecs)
        , _root(parseBuildFile())
    {}


    Token Parser::eat(std::string const& tokenType) {
        Token eaten = _lookAhead;
        if (eaten.type == "") syntaxError(tokenType);
        if (eaten.type != tokenType) syntaxError(tokenType);
        _tokenizer.readNextToken(_lookAhead);
        return eaten;
    }

    void Parser::syntaxError(std::string const& tokenType) {
        std::stringstream ss;
        ss
            << "Unexpected token: " << _lookAhead.type << ", "
            << "expected token: " << tokenType
            << std::endl;
        throw std::exception(ss.str().c_str());
    }

    std::shared_ptr<SyntaxTree::BuildFile> Parser::parseBuildFile() {
        _tokenizer.readNextToken(_lookAhead);
        auto file = std::make_shared<SyntaxTree::BuildFile>();
        while (_lookAhead.type != "eos") {
            if (_lookAhead.type == "rule") {
                file->add(parseRule());
            } // else if variable, etc
        }
        return file;
    }

    std::shared_ptr<SyntaxTree::Rule> Parser::parseRule() {
        eat("rule"); // Return value not needed
        auto rule = std::make_shared<SyntaxTree::Rule>();
        if (_lookAhead.type == "foreach") {
            rule->forEach = true;
            eat("foreach");
        }
        rule->add(parseInputs());
        rule->add(parseScript());
        rule->add(parseOutputs());
        return rule;
    }

    std::shared_ptr<SyntaxTree::Inputs> Parser::parseInputs() {
        auto inputs = std::make_shared<SyntaxTree::Inputs>();
        while (_lookAhead.type != "script") {
            inputs->add(parseInput());
        }
        return inputs;
    }
    std::shared_ptr<SyntaxTree::Input> Parser::parseInput() {
        bool exclude = _lookAhead.type == "not";
        if (exclude) eat("not");
        auto pathToken = eat("glob");
        auto input = std::make_shared<SyntaxTree::Input>(exclude, pathToken.value);
        return input;
    }

    std::shared_ptr<SyntaxTree::Script> Parser::parseScript() {
        Token token = eat("script");
        auto cmd = std::make_shared<SyntaxTree::Script>();
        cmd->script = token.value;
        return cmd;
    }

    std::shared_ptr<SyntaxTree::Outputs> Parser::parseOutputs() {
        auto outputs = std::make_shared<SyntaxTree::Outputs>();
        while (_lookAhead.type != "eos") {
            outputs->add(parseOutput());
        }
        return outputs;
    }
    std::shared_ptr<SyntaxTree::Output> Parser::parseOutput() {
        Token token = eat("glob");
        auto output = std::make_shared<SyntaxTree::Output>();
        output->path = token.value;
        return output;
    }
}