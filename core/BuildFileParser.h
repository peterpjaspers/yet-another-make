#pragma once

#include "BuildFileTokenizer.h"
#include "BuildFile.h"
#include <filesystem>
#include <string>
#include <vector>
#include <memory>

namespace YAM
{
    class __declspec(dllexport) BuildFileParser
    {
    public:
        BuildFileParser(std::filesystem::path const& buildFilePath);
        BuildFileParser(std::string const& buildFileContent);
        BuildFileParser(std::string && buildFileContent);

        std::shared_ptr<BuildFile::File> const& file() { return _file; }


    private:
        Token eat(std::string const& tokenType);
        void syntaxError(std::string const& tokenType);

        std::shared_ptr<BuildFile::File> parseBuildFile();
        std::shared_ptr<BuildFile::Rule> parseRule();
        void parseInputs(BuildFile::Inputs& inputs);
        void parseInput(BuildFile::Input& input);
        void parseScript(BuildFile::Script& script);
        void parseOutputs(BuildFile::Outputs& outputs);
        void parseOutput(BuildFile::Output& output);

        BuildFileTokenizer _tokenizer;
        Token _lookAhead;
        std::shared_ptr<BuildFile::File> _file;
    };
}


