#pragma once

#include "BuildFileTokenizer.h"
#include "BuildFile.h"
#include <filesystem>
#include <string>
#include <vector>
#include <memory>

namespace YAM
{
    // Parses a text in buildfile syntax into a BuildFile::File.
    // Buildfile syntax is inspired by tup buildfile syntax, see 
    // https://gittup.org/tup/manual.html
    //
    /*
    
    {A B}*  => 0, 1 or more times A B
    {A B}+  => 1 or more times A B
    [A B]   => optional A B
    A|  B   => A or B

    BuildFile => {Rule}*
    Rule => ':' [foreach] [CmdInputs] [ '|' OrderOnlyInputs ] '|>' Script '|>' [Outputs]
    CmdInputs => Inputs
    Inputs => [Input]*
    Input => Glob | '^'Glob | Path | '^'Path
    OrderOnlyInputs => Inputs
    Script => { identifier | InputPathFlag | OutputPathFlag }+
    InputPathFlag => '%f' | '%b' | '%B' | '%e' | '%D' | '%d'
    OutputPathFlag => '%o'
    Outputs => { RelPath | InputPathFlag }+
    Path => RelPath | RepoPath
    RelPath => identifier [ { '\'identifier }* ]
    RepoPath => '<' RepoName '>\' RelPath
    RepoName => identifier

    InputPathFlag semantics:
        %f If not a foreach rule: CmdInputs, e.g. c/d.e  c/f.g
        %f If a foreach rule: current cmd input path, e.g. c/d.e
        %i OrderOnlyInputs
        %b -> d.e (from cmd input c/d.e)
        %B -> d   (from cmd input c/d.e)
        %e -> e   (from cmd inputa c/d.e)
        %d -> b/c (from cmd input b/c/d.e)
        %D -> c   (from cmd input c/d.e)

    OutputPathFlag semantics:
        %o -> Outputs

    Example rules:
    # Rules to compile foo.c and bar.c into foo.o and bar.o
    : foo.c |> gcc -c foo.c -o foo.o |> foo.o
    : bar.c |> gcc -c bar.c -o bar.o |> bar.o

    # Same as above but using foreach and %-flags
    : foreach foo.c bar.c |> gcc -c %f -o %o |> %B.o

    # Rule to compile all .c files into object files
    : foreach *.c |> gcc -c %f -o %o |> %B.o

    # Rule to compile all .c files except main.c into object files
    : foreach *.c ^main.c |> gcc -c %f -o %o |> %B.o

    */
    class __declspec(dllexport) BuildFileParser
    {
    public:
        BuildFileParser(std::filesystem::path const& buildFilePath);
        BuildFileParser(std::string const& buildFileContent, std::filesystem::path const& buildFilePath = "");
        BuildFileParser(std::string && buildFileContent, std::filesystem::path const& buildFilePath = "");

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

        std::filesystem::path _buildFilePath;
        BuildFileTokenizer _tokenizer;
        Token _lookAhead;
        std::shared_ptr<BuildFile::File> _file;
    };
}


