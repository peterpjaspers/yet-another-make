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
        std::ifstream file(path.string().c_str());
        std::stringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

    TokenRegexSpec const* findTokenSpec(std::string tokenType) {
        for (auto t : BuildFileTokenSpecs::specs()) {
            if (t->type() == tokenType) return t;
        }
        throw std::runtime_error("unknown token type " + tokenType);
    }

    std::vector<ITokenSpec const*> ispecs = BuildFileTokenSpecs::ispecs();
    std::vector<TokenRegexSpec const*> tokenSpecs = BuildFileTokenSpecs::specs();
    TokenRegexSpec const* whiteSpace(BuildFileTokenSpecs::whiteSpace());
    TokenRegexSpec const* comment1(BuildFileTokenSpecs::comment1());
    TokenRegexSpec const* commentN(BuildFileTokenSpecs::commentN());
    TokenRegexSpec const* depBuildFile(BuildFileTokenSpecs::depBuildFile());
    TokenRegexSpec const* depGlob(BuildFileTokenSpecs::depGlob());
    TokenRegexSpec const* rule(BuildFileTokenSpecs::rule());
    TokenRegexSpec const* foreach(BuildFileTokenSpecs::foreach());
    TokenRegexSpec const* ignore(BuildFileTokenSpecs::ignore());
    TokenRegexSpec const* curlyOpen(BuildFileTokenSpecs::curlyOpen());
    TokenRegexSpec const* curlyClose(BuildFileTokenSpecs::curlyClose());
    TokenRegexSpec const* cmdStart(BuildFileTokenSpecs::cmdStart());
    TokenRegexSpec const* cmdEnd(BuildFileTokenSpecs::cmdEnd());
    TokenRegexSpec const* script(BuildFileTokenSpecs::script());
    TokenRegexSpec const* vertical(BuildFileTokenSpecs::vertical());
    TokenRegexSpec const* glob(BuildFileTokenSpecs::glob());

    static std::vector<ITokenSpec const*> skip = {whiteSpace, comment1, commentN};

    std::vector<ITokenSpec const*> tspecs(std::vector<ITokenSpec const*> const& s) {
        std::vector<ITokenSpec const*> specs = skip;
        specs.insert(specs.end(), s.begin(), s.end());
        return specs;
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
        _tokenizer.readNextToken(skip);
        _lookAhead.spec = nullptr;
        if (!specs.empty()) _lookAhead = _tokenizer.readNextToken(specs);
    }

    Token BuildFileParser::eat(
        ITokenSpec const* toEat,
        std::vector<ITokenSpec const*> const& toLookAhead
     ) {
        if (toEat != nullptr && _lookAhead.spec != toEat) {
            syntaxError();
        }
        Token eaten = _lookAhead;
        lookAhead(toLookAhead);
        return eaten;
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
        lookAhead({ depBuildFile, depGlob });
        if (_lookAhead.spec == depBuildFile || _lookAhead.spec == depGlob) {
            parseDeps(file->deps);
        }
        lookAhead({ rule });
        while (
            _lookAhead.spec == rule 
            // || _lookAhead.spec == var, etc
         ) {
             file->variablesAndRules.push_back(parseRule());
             lookAhead({ rule });
         }
         return file;
    }

    void BuildFileParser::parseDeps(BuildFile::Deps& deps) {
        deps.line = _tokenizer.tokenStartLine();
        deps.column = _tokenizer.tokenStartColumn();
        while (_lookAhead.spec == depBuildFile || _lookAhead.spec == depGlob) {
            if (_lookAhead.spec == depBuildFile) {
                eat(depBuildFile, { glob });
                std::filesystem::path path = parsePath();
                deps.depBuildFiles.push_back(path);
                eat(glob, { depBuildFile, depGlob });
            } else if (_lookAhead.spec == depGlob) {
                eat(depGlob, { glob });
                std::filesystem::path path = parseGlob();
                deps.depGlobs.push_back(path);
                eat(glob, { depBuildFile, depGlob });
            }
        }
    }

    std::shared_ptr<BuildFile::Rule> BuildFileParser::parseRule() {
        auto rulePtr = std::make_shared<BuildFile::Rule>();
        rulePtr->line = _tokenizer.tokenStartLine();
        rulePtr->column = _tokenizer.tokenStartColumn();
        eat(rule, { foreach, curlyOpen, ignore, glob, vertical, script });
        if (_lookAhead.spec == foreach) {
            rulePtr->forEach = true;
            eat(foreach, {  curlyOpen, ignore, glob, vertical, script });
        }
        if (
            _lookAhead.spec == curlyOpen
            || _lookAhead.spec == ignore
            || _lookAhead.spec == glob
        ) {
            parseInputs(rulePtr->cmdInputs, { curlyOpen, ignore, glob, vertical, script });
        }
        if (_lookAhead.spec == vertical) {
            parseOrderOnlyInputs(rulePtr->orderOnlyInputs, { curlyOpen, ignore, glob, script });
        }
        if (_lookAhead.spec != script) syntaxError();
        parseScript(rulePtr->script);
        if (_lookAhead.spec == glob) {
            parseOutputs(rulePtr->outputs);
        }
        if (_lookAhead.spec == curlyOpen) {
            parseGroup(rulePtr->outputGroup);
        }
        return rulePtr;
    }

    void BuildFileParser::parseInputs(BuildFile::Inputs& inputs, std::vector<ITokenSpec const*> const& toLookAhead) {
        inputs.line = _tokenizer.tokenStartLine();
        inputs.column = _tokenizer.tokenStartColumn();
        while (
            _lookAhead.spec == curlyOpen
            || _lookAhead.spec == ignore
            || _lookAhead.spec == glob
        ) {
            BuildFile::Input in;
            parseInput(in);
            inputs.inputs.push_back(in);
            eat(nullptr, toLookAhead);
        }
    }

    void BuildFileParser::parseInput(BuildFile::Input& input) {
        input.line = _tokenizer.tokenStartLine();
        input.column = _tokenizer.tokenStartColumn();
        if (_lookAhead.spec == curlyOpen) {
            eat(curlyOpen, {glob});
            input.exclude = false;
            input.isGroup = true;
            input.pathPattern = parsePath();
            eat(glob, { curlyClose });
        } else if (_lookAhead.spec == ignore) {
            eat(ignore, { glob });
            input.isGroup = false;
            input.exclude = true;
            input.pathPattern = parseGlob();
            eat({ glob }, { });
        } else if (_lookAhead.type == "glob") {
            input.isGroup = false;
            input.exclude = false;
            input.pathPattern = parseGlob();
            eat(glob, {});
        }
    }

    void BuildFileParser::parseOrderOnlyInputs(BuildFile::Inputs& inputs, std::vector<ITokenSpec const*> const& toLookAhead) {
        inputs.line = _tokenizer.tokenStartLine();
        inputs.column = _tokenizer.tokenStartColumn();
        if (_lookAhead.spec == vertical) {
            eat(vertical, toLookAhead);
            parseInputs(inputs, toLookAhead);
        }
    }

    void BuildFileParser::parseScript(BuildFile::Script& scriptNode) {
        scriptNode.line = _tokenizer.tokenStartLine();
        scriptNode.column = _tokenizer.tokenStartColumn();
        Token token = eat(script, { glob, curlyOpen, _tokenizer.eosTokenSpec()});
        scriptNode.script = token.value;
    }

    void BuildFileParser::parseOutputs(BuildFile::Outputs& outputs) {
        outputs.line = _tokenizer.tokenStartLine();
        outputs.column = _tokenizer.tokenStartColumn();
        while (_lookAhead.spec == glob || _lookAhead.spec == ignore) {
            BuildFile::Output out;
            parseOutput(out);
            outputs.outputs.push_back(out);
        }
    }
    void BuildFileParser::parseOutput(BuildFile::Output& output) {
        output.line = _tokenizer.tokenStartLine();
        output.column = _tokenizer.tokenStartColumn();
        output.ignore = false;
        if (_lookAhead.spec == glob) {
            output.path = parsePath();
            eat(glob, { ignore, glob, curlyOpen });
        } else if (_lookAhead.spec == ignore) {
            output.ignore = true;
            eat(ignore, { glob, curlyOpen });
        } 
    }

    void BuildFileParser::parseGroup(std::filesystem::path& groupName) {
        if (_lookAhead.spec == curlyOpen) {
            eat(curlyOpen, { glob });
            groupName = parsePath();
            eat(glob, { curlyClose });
        }
    }

    std::filesystem::path BuildFileParser::parseGlob() {
        return _lookAhead.value;
    }

    std::filesystem::path BuildFileParser::parsePath() {
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
        }
        return _lookAhead.value;
    }
}