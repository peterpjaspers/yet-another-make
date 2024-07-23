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
    // 
    // Buildfile syntax is inspired by tup buildfile syntax, see 
    // https://gittup.org/tup/manual.html
    // Syntax and semantics however are NOT tup compatible.
    //
    // TODO: not all syntax as describe below is implemented:
    //    ExtraOutput
    //    Ignored output is recognized by path/glob preceeded by '^'.
    //    Optional output is recognized as glob.
    /*
    
    Syntactical symbols:
    A*  => 0, 1 or more times A
    A+  => 1 or more times
    [A] => optional A
    A|B => A or B
    () group symbols, e.g. (A|B)*

    Syntax:

        BuildFile :== {Dependency}* {Rule}*

        Dependency :== DepBuildFile | DepGlob
        DepBuildFile :== 'buildfile' BuildFilePath | BuildFileDirPath | BuildFileGlob
        BuildFilePath :== Path // path of buildfile
        BuildFileDirPath :== Path // path of directory that contains buildfile
        BuildFileGlob :== Path // with glob characters, matches directories or buildfiles
        DepGlob :== 'glob' Glob

        Rule :== ':' [foreach] [CmdInputs] [ '|' OrderOnlyInputs ] '|>' Script '|>' [CmdOutputs}
        CmdInputs :== Input*
        Input :== Path | Glob | Exclude | [Group]
        Exclude :== '^'Path | '^'Glob
        Glob :== Path // with glob special characters (* ? [])
        OrderOnlyInputs :== ( Path | Group )*
        Script :== ( identifier | InputPathFlag | OutputPathFlag )*
        InputPathFlag :== %[Index] 'f' | '%b' | '%B' | '%e' | '%D' | '%d'
        Index :== integer
        OutputPathFlag :== '%o'
        CmdOutputs :== CmdOutput+  (Group | Bin )*
        CmdOutput :== Output | ExtraOutput | OptionalOutput | IgnoreOutput
        Output :== 'out=' Path (',' Path])* // paths optionally contain InputPathFlags
        ExtraOutput :== 'extra=' Path (',' Path])* // paths optionally contain InputPathFlags
        OptionalOutput :== 'opt=' (Path|Glob) (',' (Path|Glob))*
        IgnoreOutput :== 'ign=' (Path|Glob) (',' (Path|Glob))*
        Path :== RelPath | SymbolicPath
        RelPath :== identifier ('/'identifier)*
        SymbolicPath :== '@@'RepoName '/' RelPath
        RepoName :== identifier // the name of a configured file repository
        Group :== Path where last path component is between angled brackets, e.g. modules\<someGroupName>
        Bin :== '{' identifier '}'

    Semantics:

    Dependency: a changed dependency triggers re-execution of the buildfile.

    DepBuildFile: buildfile A depends on buildfile B when A uses inputs that are
    outputs of rules in B. The buildfile path is the path of the directory that
    contains the buildfile. This is convenient because the extension of the
    buildfile may vary across directories and over time (because buildfile 
    language can be changed, cause buildfile extension to change. E.g. from
    .bat to .py).

    DepGlob: buildfile A depends on glob G when A produces a set of rules that
    depends on the set of files that match G. E.g. the buildfile produces a 
    rule for each .cpp file in a the src directory. In that case there is a
    dependency on glob src\*.cpp. Note: the buildfile could also have produced
    a single foreach rule with input src\*.cpp. In that case yam evaluates the
    glob and adds the glob as a dependency of the buildfile.

    Group: a set of file paths.
    A group is populated by commands derived from rules that specify the group
    in their output section. These commands add their mandatory, extra mandatory
    and optional output paths to the group. 

    Glob: a path that contains glob special characters, see class Glob. Globs
    are supported for both source and output (generated) files. A glob on 
    output files is evaluated against the set of output files defined by rules
    in buildfiles that are declared in the DepBuildFile section.

    CmdInputs: yam expands the Globs and Groups into a list of file paths.
    An Exclude removes all paths previously included that match the Exclude.
    Paths and Globs can reference source files and/or generated files that are
    mandatory outputs of earlier defined rules. It is not possible to reference
    optional outputs of rules.
    Yam ensures that all generated input files are made up-to-date before 
    executing the command script. This includes all mandatory and optional
    outputs that were added to the input groups.
    Note: optional outputs in groups ensure proper build-ordering. Although
    it is not possible to reference optional outputs in the input section,
    it is possible to reference optional output paths in the script section.

    OrderOnlyInputs: a list of generated files. Yam ensures that these files
    are made up-to-date before executing the command script.

    RelPath: a relative path, relative to the directory that contains the
    buildfile.

    SymbolicPath: in a symbolic path the repository root directory is replaced 
    by the repository name. In buildfiles a symbolic path is used to reference
    a file in another repository. This allows repositories to be moved to other 
    directories without having to modify buildfiles.

    Repository: a directory tree that contains buildfiles and/or source files
    and/or files generated by yam.

    InputPathFlags:
    The %f, %b, %B, %e, %d and %D flags transform one or more paths in the
    expanded CmdInputs. In a foreach rule they transform the current file path
    in the cmd inputs. In a non-foreach rule they transform all paths in the 
    cmd inputs.

        %f Path, e.g. c/d.e -> c/d.e
        %b File name, e.g. c/d.e -> d.e
        %B File base name, e.g. c/d.e -> d
        %e File extension, e.g. c/d.e -> e
        %d Directory, e.g b/c/d.e -> b/c
        %D Lowest level directory, e.g. b/c/d.e -> c

    The %i flag selects all file paths in the expanded OrderOnlyInputs.
        %i Path, e.g. c/d.e -> c/d.e
    
    Index: in case the %-flag is followed by integer idx then the %-flag
    transforms ExpandedCmdInputs[idx] cq ExpandedOrderOnlyInputs[idx].
    E.g. expanded inputs are (pathA, pathB, pathC), then %2b tranforms pathC
    into file name of pathC.

    Rule: defines a command script that transforms input files to output files.
    Rule order matters: a rule that defines output A must be defined before a
    rule can use A as input.

    OutputPathFlag semantics:
        %o -> references all (non-extra) mandatory outputs
        %[i]0 -> references the i-th (non-extra) mandatory output.
    
    Example rules:

    # Rule to compile main.c and bar.c and link the resulting
    # object files into an executable program
    : |> gcc -c main.c -o main.o |> out=main.o
    : |> gcc -c bar.c -o bar.o |> out=bar.o
    : main.obj bar.obj |> gcc main.obj bar.obj -o program |> out=program.exe

    # Same as above but using foreach and %-flags and collecting all object
    # files into a bin
    : foreach main.c bar.c |> gcc -c %f -o %o |> out=%B.o {objs}
    : {objs} |> gcc main.obj bar.obj -o program |> out=program.exe

    # Same as above but compiling all cpp files in src directory and
    # collectiong all object files in a group
    : foreach src/*.c |> gcc -c %f -o %o |> out=%B.o ./<objects>
    : ./<objects> |> gcc %f -o %o |> out=program.exe

    # Two buildfiles: one that compiles all cpp files and collects the
    # resulting objects files in a group and one that links the (object
    # files in the) group into an executable program.
    #
    # <repoRoot>/p/q/buildfile_yam.txt:
    : foreach src/*.c |> gcc -c %f -o %o |> out=%B.o ./<objects>

    # <repoRoot>/p/r/buildfile_yam.txt:
    : ../q/<objects> |> gcc %f -o %o |> program.exe

    */

    class __declspec(dllexport) BuildFileParser
    {
    public:
        BuildFileParser(std::filesystem::path const& buildFilePath);
        BuildFileParser(std::string const& buildFileContent, std::filesystem::path const& buildFilePath = "test");
        BuildFileParser(std::string && buildFileContent, std::filesystem::path const& buildFilePath = "test");

        std::shared_ptr<BuildFile::File> const& file() { return _file; }

    private:
        void lookAhead(std::vector<ITokenSpec const*> const& specs);
        Token eat(ITokenSpec const* toEat);
        void syntaxError();

        std::shared_ptr<BuildFile::File> parseBuildFile();
        void eatDep(BuildFile::Deps& deps);
        void eatDepBuildFile(BuildFile::Deps& deps);
        void eatDepGlob(BuildFile::Deps& deps);
        std::shared_ptr<BuildFile::Rule> eatRule();
        void parseInputs(BuildFile::Inputs& inputs);
        void eatInput(BuildFile::Input& input);
        void parseOrderOnlyInputs(BuildFile::Inputs& inputs);
        void eatScript(BuildFile::Script& script);
        void parseOutputs(BuildFile::Outputs& outputs);
        void eatOutput(BuildFile::Output& output);
        Token eatGlobToken();
        std::filesystem::path eatGlob();
        std::filesystem::path eatPath();

        std::filesystem::path _buildFilePath;
        BuildFileTokenizer _tokenizer;
        Token _lookAhead;
        std::shared_ptr<BuildFile::File> _file;
    };
}


