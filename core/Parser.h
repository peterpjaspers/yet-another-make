#pragma once

#include "Tokenizer.h"
#include "SyntaxTree.h"
#include <filesystem>
#include <string>
#include <vector>
#include <memory>

namespace YAM
{
    class __declspec(dllexport) Parser
    {
    public:
        Parser(std::filesystem::path const& buildFilePath);
        Parser(std::string const& buildFileContent);
        Parser(std::string && buildFileContent);

        std::shared_ptr<SyntaxTree::Node> const& syntaxTree() { return _root; }


    private:
        Token eat(std::string const& tokenType);
        void syntaxError(std::string const& tokenType);

        std::shared_ptr<SyntaxTree::BuildFile> parseBuildFile();
        std::shared_ptr<SyntaxTree::Rule> parseRule();
        std::shared_ptr<SyntaxTree::Inputs> parseInputs();
        std::shared_ptr<SyntaxTree::Input> parseInput();
        std::shared_ptr<SyntaxTree::Script> parseScript();
        std::shared_ptr<SyntaxTree::Outputs> parseOutputs();
        std::shared_ptr<SyntaxTree::Output> parseOutput();

        std::string _content;
        Tokenizer _tokenizer;
        Token _lookAhead;
        std::shared_ptr<SyntaxTree::Node> _root;
    };
}


