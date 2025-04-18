#include "../BuildFileCompiler.h"
#include "../PercentageFlagsCompiler.h"
#include "../FileRepositoryNode.h"
#include "../DirectoryNode.h"
#include "../SourceFileNode.h"
#include "../GeneratedFileNode.h"
#include "../GroupNode.h"
#include "../CommandNode.h"
#include "../ForEachNode.h"
#include "../GlobNode.h"
#include "../RepositoriesNode.h"
#include "../ExecutionContext.h"
#include "../FileSystem.h"
#include "../Globber.h" 

#include "executeNode.h"
#include "DirectoryTree.h"
#include "gtest/gtest.h"

#include <fstream>

namespace
{
    using namespace YAM;
    using namespace YAMTest;

    std::map<std::filesystem::path, std::shared_ptr<CommandNode>> emptyCmds;
    std::map<std::filesystem::path, std::shared_ptr<ForEachNode>> emptyForEachNodes;
    std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>> emptyOutputs;
    std::map<std::filesystem::path, std::shared_ptr<GroupNode>> emptyGroups;

    class CompilerSetup
    {
    public:

        std::string repoName;
        std::filesystem::path repoDir;
        DirectoryTree testTree;
        ExecutionContext context;
        std::shared_ptr<FileRepositoryNode> repo;
        std::shared_ptr<SourceFileNode> mainFile;
        std::shared_ptr<SourceFileNode> lib1File;
        std::shared_ptr<SourceFileNode> lib2File;

        CompilerSetup()
            : repoName("repo")
            , repoDir(FileSystem::createUniqueDirectory())
            , testTree(repoDir, 1, RegexSet({ }))
        {
            //context.threadPool().size(1);
            std::filesystem::create_directory(repoDir / "src");
            std::filesystem::create_directory(repoDir / "output");

            std::filesystem::path f1(repoDir / "src\\main.cpp");
            std::filesystem::path f2(repoDir / "src\\lib1.cpp");
            std::filesystem::path f3(repoDir / "src\\lib2.cpp");
            std::ofstream s1(f1.string().c_str());
            s1 << "void main() {}" << std::endl;
            s1.close();
            std::ofstream s2(f2.string().c_str());
            s2 << "void lib1() {}" << std::endl;
            s2.close();
            std::ofstream s3(f3.string().c_str());
            s2 << "void lib2() {}" << std::endl;
            s3.close();

            repo = std::make_shared<FileRepositoryNode>(&context, "repo", repoDir, FileRepositoryNode::RepoType::Build);
            auto repos = std::make_shared<RepositoriesNode>(&context, repo);
            context.repositoriesNode(repos);
            bool completed = YAMTest::executeNode(repo->directoryNode().get());
            EXPECT_TRUE(completed);
            mainFile = dynamic_pointer_cast<SourceFileNode>(context.nodes().find(repo->symbolicPathOf(f1)));
            lib1File = dynamic_pointer_cast<SourceFileNode>(context.nodes().find(repo->symbolicPathOf(f2)));
            lib2File = dynamic_pointer_cast<SourceFileNode>(context.nodes().find(repo->symbolicPathOf(f3)));
            EXPECT_NE(nullptr, mainFile);
            EXPECT_NE(nullptr, lib1File);
            EXPECT_NE(nullptr, lib2File);
        }
    };


    void verify(
        CompilerSetup& setup,
        BuildFileCompiler const& compiler,
        BuildFile::Output const& ignoredOutput,
        std::filesystem::path const& globNameSpace
    ) {
        auto const& commands = compiler.commands();
        ASSERT_EQ(2, commands.size());
        auto command0 = (commands.begin())->second;
        auto command1 = (++commands.begin())->second;

        std::filesystem::path repoRoot("@@repo");
        ASSERT_EQ(1, command0->cmdInputs().size());
        auto input00 = command0->cmdInputs()[0];
        EXPECT_EQ(setup.lib1File, input00);
        EXPECT_EQ(3, command0->orderOnlyInputs().size());
        EXPECT_EQ("type %f > %o", command0->script());

        ASSERT_EQ(1, command0->mandatoryOutputs().size());
        auto mouts0 = command0->mandatoryOutputsVec();
        auto output00 = mouts0[0];
        EXPECT_EQ("@@repo\\output\\lib1.obj", output00->name().string());

        std::vector<CommandNode::OutputFilter> const &filters0 = command0->outputFilters();
        ASSERT_EQ(2, filters0.size());
        EXPECT_EQ(CommandNode::OutputFilter::Output, filters0[0]._type);
        EXPECT_EQ("@@repo\\output\\lib1.obj", filters0[0]._path);
        EXPECT_EQ(CommandNode::OutputFilter::Ignore, filters0[1]._type);
        EXPECT_EQ(std::filesystem::path("@@repo") / ignoredOutput.path, filters0[1]._path);

        ASSERT_EQ(1, command1->cmdInputs().size());
        auto input10 = command1->cmdInputs()[0];
        EXPECT_EQ(setup.lib2File, input10);
        EXPECT_EQ("type %f > %o", command1->script());

        ASSERT_EQ(1, command1->mandatoryOutputs().size());
        auto mouts1 = command1->mandatoryOutputsVec();
        auto output10 = mouts1[0];
        EXPECT_EQ("@@repo\\output\\lib2.obj", output10->name().string());

        std::vector<CommandNode::OutputFilter> const& filters1 = command1->outputFilters();
        ASSERT_EQ(2, filters1.size());
        EXPECT_EQ(CommandNode::OutputFilter::Output, filters1[0]._type);
        EXPECT_EQ("@@repo\\output\\lib2.obj", filters1[0]._path);
        EXPECT_EQ(CommandNode::OutputFilter::Ignore, filters1[1]._type);
        EXPECT_EQ(std::filesystem::path("@@repo") / ignoredOutput.path, filters1[1]._path);

        auto const& globs = compiler.inputGlobs();
        ASSERT_EQ(2, globs.size());
        std::filesystem::path depGlobName(globNameSpace / setup.repo->directoryNode()->name() / "*.h");
        std::filesystem::path ruleGlobName(globNameSpace / setup.repo->directoryNode()->name() / "src\\*.cpp");
        auto const& depGlobIt = globs.find(depGlobName);
        auto const& ruleGlobIt = globs.find(ruleGlobName);
        ASSERT_TRUE(globs.end() != depGlobIt);
        ASSERT_TRUE(globs.end() != ruleGlobIt);
        std::shared_ptr<GlobNode> depGlob = globs.find(depGlobName)->second;
        std::shared_ptr<GlobNode> ruleGlob = globs.find(ruleGlobName)->second;

        ASSERT_NE(nullptr, ruleGlob);
        auto ruleGlobBaseDirName = setup.repo->directoryNode()->name() / "src";
        EXPECT_EQ(ruleGlobBaseDirName, ruleGlob->baseDirectory()->name());
        EXPECT_EQ("*.cpp", ruleGlob->pattern());

        ASSERT_NE(nullptr, depGlob);
        auto depGlobBaseDirName = setup.repo->directoryNode()->name();
        EXPECT_EQ(depGlobBaseDirName, depGlob->baseDirectory()->name());
        EXPECT_EQ("*.h", depGlob->pattern());

        auto const& groups = compiler.outputGroups();
        ASSERT_EQ(1, groups.size());
        auto groupIt = groups.find("@@repo\\outputGroup1");
        ASSERT_TRUE(groups.end() != groupIt);
        auto const& grpContent = groupIt->second->content();
        EXPECT_NE(grpContent.end(), std::find(grpContent.begin(), grpContent.end(), command0));
        EXPECT_NE(grpContent.end(), std::find(grpContent.begin(), grpContent.end(), command1));
    }

    // TODO
    // : a.cpp b.cpp |> cc %1f -o %1o & cc %2f -o %2o |> %1B.obj %2B.obj
    // : foreach *.cpp |> cc %f -i %o |> %B.obj
    //
    // Error message tests
    TEST(BuildFileCompiler, foreach) {
        CompilerSetup setup;
        BuildFile::Input input;
        input.exclude = false;
        input.pathType = BuildFile::PathType::Glob;
        input.path = "src\\*.cpp";
        BuildFile::Input excludedInput;
        excludedInput.exclude = true;
        excludedInput.pathType = BuildFile::PathType::Path;
        excludedInput.path = "src\\main.cpp";
        BuildFile::Output output;
        output.ignore = false;
        output.path = "output\\%1B.obj";
        output.pathType = BuildFile::PathType::Path;
        BuildFile::Output ignoredOutput;
        ignoredOutput.ignore = true;
        ignoredOutput.pathType = BuildFile::PathType::Path;
        ignoredOutput.path = R"(.*\.dep)";
        auto rule = std::make_shared<BuildFile::Rule>();
        rule->forEach = true;
        rule->cmdInputs.inputs.push_back(input);
        rule->cmdInputs.inputs.push_back(excludedInput);
        rule->orderOnlyInputs.inputs.push_back(input);
        rule->script.script = "type %f > %o";
        rule->outputs.outputs.push_back(output);
        rule->outputs.outputs.push_back(ignoredOutput);
        BuildFile::File file;
        file.buildFile = "buildFile_yam.txt";
        file.deps.depGlobs.push_back("*.h");
        file.variablesAndRules.push_back(rule);
        rule->outputGroups.push_back("outputGroup1");

        std::filesystem::path globNameSpace("private");
        BuildFileCompiler compiler1(
            &setup.context,
            setup.repo->directoryNode(), file,
            emptyCmds, emptyForEachNodes, 
            emptyOutputs, emptyGroups,
            std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>>(),
            globNameSpace);
        verify(setup, compiler1, ignoredOutput, globNameSpace);

        for (auto const& pair : compiler1.commands()) {
            setup.context.nodes().add(pair.second);
        }
        for (auto const& pair : compiler1.inputGlobs()) {
            setup.context.nodes().add(pair.second);
        }
        for (auto const& pair : compiler1.mandatoryOutputs()) {
            setup.context.nodes().add(pair.second);
        }
        for (auto const& pair : compiler1.outputGroups()) {
            setup.context.nodes().add(pair.second);
        }
        BuildFileCompiler compiler2(
            &setup.context,
            setup.repo->directoryNode(), file,
            compiler1.commands(), compiler1.forEachNodes(), 
            compiler1.mandatoryOutputs(), compiler1.outputGroups(),
            std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>>(),
            globNameSpace);
        EXPECT_EQ(compiler1.commands(), compiler2.commands());
        EXPECT_EQ(compiler1.forEachNodes(), compiler2.forEachNodes());
        EXPECT_EQ(compiler1.mandatoryOutputs(), compiler2.mandatoryOutputs());
        EXPECT_EQ(compiler1.outputGroups(), compiler2.outputGroups());
        verify(setup, compiler2, ignoredOutput, globNameSpace);
    }

    TEST(BuildFileCompiler, singleInAndOutput) {
        CompilerSetup setup;
        BuildFile::Input input;
        input.exclude = false;
        input.pathType = BuildFile::PathType::Path;
        input.path = "src\\main.cpp";
        BuildFile::Output output;
        output.ignore = false;
        output.pathType = BuildFile::PathType::Path;
        output.path = "output\\main.obj";
        auto rule = std::make_shared<BuildFile::Rule>();
        rule->forEach = true;
        rule->cmdInputs.inputs.push_back(input);
        rule->script.script = "cc main.cpp -o main.obj";
        rule->outputs.outputs.push_back(output);
        BuildFile::File file;
        file.buildFile = "buildFile_yam.txt";
        file.variablesAndRules.push_back(rule);

        BuildFileCompiler compiler(
            &setup.context, 
            setup.repo->directoryNode(), file,
            emptyCmds, emptyForEachNodes,
            emptyOutputs, emptyGroups,
            std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>>());
        auto const& commands = compiler.commands();
        ASSERT_EQ(1, commands.size());
        auto command0 = (commands.begin())->second;

        ASSERT_EQ(1, command0->cmdInputs().size());
        auto input00 = command0->cmdInputs()[0];
        EXPECT_EQ(setup.mainFile, input00);
        ASSERT_EQ(rule->script.script, command0->script());
        auto mouts0 = command0->mandatoryOutputsVec();
        ASSERT_EQ(1, mouts0.size());
        auto output00 = mouts0[0];
        EXPECT_EQ(std::string("@@repo\\output\\main.obj"), output00->name().string());
    }


    TEST(BuildFileCompiler, emptyFile) {
        BuildFile::File file;
        ExecutionContext context;
        std::shared_ptr<DirectoryNode> baseDir;
        BuildFileCompiler compiler(
            &context, baseDir, file,
            emptyCmds, emptyForEachNodes,
            emptyOutputs, emptyGroups,
            std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>>());
        EXPECT_EQ(0, compiler.commands().size());
    }

    TEST(BuildFileCompiler, scriptOnly) {
        BuildFile::File file;
        ExecutionContext context;
        auto baseDir = std::make_shared<DirectoryNode>(&context, "base", nullptr);
        
        auto rule = std::make_shared<BuildFile::Rule>();
        rule->forEach = false;
        BuildFile::Script script;
        script.script = R"(
echo hello world
echo %%EV%%
)";
        rule->script = script;
        file.variablesAndRules.push_back(rule);

        BuildFileCompiler compiler(
            &context, baseDir, file,
            emptyCmds, emptyForEachNodes,
            emptyOutputs, emptyGroups,
            std::map<std::filesystem::path, std::shared_ptr<GeneratedFileNode>>());
        EXPECT_EQ(1, compiler.commands().size());
        std::shared_ptr<CommandNode> cmd = compiler.commands().begin()->second;
        EXPECT_EQ(script.script, cmd->script());
    }

    TEST(BuildFileCompiler, multilineScript) {
        BuildFile::File file;
        ExecutionContext context;
        std::shared_ptr<DirectoryNode> baseDir;

        auto rule = std::make_shared<BuildFile::Rule>();
        rule->forEach = false;
        BuildFile::Script script;
        script.script = R"(
echo hello world
echo %%EV%%
)";
        rule->script = script;
        file.variablesAndRules.push_back(rule);
        file.buildFile = "test";

        PercentageFlagsCompiler compiler(
            file.buildFile,
            script,
            baseDir,
            std::vector<std::shared_ptr<Node>>(),
            std::vector<std::shared_ptr<Node>>(),
            std::vector<std::shared_ptr<GeneratedFileNode>>());
        std::string expectedScript = R"(
echo hello world
echo %EV%
)";
        EXPECT_EQ(expectedScript, compiler.result());
    }
}