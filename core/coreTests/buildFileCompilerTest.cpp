#include "../BuildFileCompiler.h"
#include "../FileRepository.h"
#include "../DirectoryNode.h"
#include "../SourceFileNode.h"
#include "../GeneratedFileNode.h"
#include "../CommandNode.h"
#include "../GlobNode.h"
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

    class CompilerSetup
    {
    public:

        std::string repoName;
        std::filesystem::path repoDir;
        DirectoryTree testTree;
        ExecutionContext context;
        std::shared_ptr<FileRepository> repo;
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

            repo = std::make_shared<FileRepository>(
                "repo",
                repoDir,
                &context);
            context.addRepository(repo);
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

    // TODO
    // : a.cpp b.cpp |> cc %1f -o %1o & cc %2f -o %2o |> %1B.obj %2B.obj
    // : foreach *.cpp |> cc %f -i %o |> %B.obj
    //
    // Error message tests
    TEST(BuildFileCompiler, foreach) {
        CompilerSetup setup;
        BuildFile::Input input;
        input.exclude = false;
        input.pathPattern = "src\\*.cpp";
        BuildFile::Input excludedInput;
        excludedInput.exclude = true;
        excludedInput.pathPattern = "src\\main.cpp";
        BuildFile::Output output;
        output.ignore = false;
        output.path = "output\\%B.obj";
        BuildFile::Output ignoredOutput;
        ignoredOutput.ignore = true;
        ignoredOutput.path = R"(.*\.dep)";
        auto rule = std::make_shared<BuildFile::Rule>();
        rule->forEach = true;
        rule->cmdInputs.inputs.push_back(input);
        rule->cmdInputs.inputs.push_back(excludedInput);
        rule->script.script = "echo hello world";
        rule->outputs.outputs.push_back(output);
        rule->outputs.outputs.push_back(ignoredOutput);
        BuildFile::File file;
        file.variablesAndRules.push_back(rule);

        BuildFileCompiler compiler(&setup.context, setup.repo->directoryNode(), file);
        auto const& commands = compiler.commands();
        ASSERT_EQ(2, commands.size());
        auto command0 = *(commands.begin());
        auto command1 = *(++commands.begin());

        ASSERT_EQ(1, command0->cmdInputs().size());
        auto input00 = command0->cmdInputs()[0];
        EXPECT_EQ(setup.lib1File, input00);
        ASSERT_EQ(rule->script.script, command0->script());
        ASSERT_EQ(1, command0->outputs().size());
        auto output00 = command0->outputs()[0];
        EXPECT_EQ(std::string("<repo>\\output\\lib1.obj"), output00->name().string());
        ASSERT_EQ(1, command0->ignoredOutputs().size());
        EXPECT_EQ(ignoredOutput.path, command0->ignoredOutputs()[0]);

        ASSERT_EQ(1, command1->cmdInputs().size());
        auto input10 = command1->cmdInputs()[0];
        EXPECT_EQ(setup.lib2File, input10);
        ASSERT_EQ(rule->script.script, command1->script());
        ASSERT_EQ(1, command1->outputs().size());
        auto output1 = command1->outputs()[0];
        EXPECT_EQ(std::string("<repo>\\output\\lib2.obj"), output1->name().string());
        ASSERT_EQ(1, command1->ignoredOutputs().size());
        EXPECT_EQ(ignoredOutput.path, command1->ignoredOutputs()[0]);

        auto const& globs = compiler.globs();
        ASSERT_EQ(1, globs.size());
        auto glob0 = *(globs.begin());
        EXPECT_EQ(input.pathPattern.filename(), glob0->pattern());
        EXPECT_EQ(setup.repo->directoryNode()->name() / input.pathPattern.parent_path(), glob0->baseDirectory()->name());
    }


    TEST(BuildFileCompiler, singleInAndOutput) {
        CompilerSetup setup;
        BuildFile::Input input;
        input.exclude = false;
        input.pathPattern = "src\\main.cpp";
        BuildFile::Output output;
        output.ignore = false;
        output.path = "output\\main.obj";
        auto rule = std::make_shared<BuildFile::Rule>();
        rule->forEach = true;
        rule->cmdInputs.inputs.push_back(input);
        rule->script.script = "cc main.cpp -o main.obj";
        rule->outputs.outputs.push_back(output);
        BuildFile::File file;
        file.variablesAndRules.push_back(rule);

        BuildFileCompiler compiler(&setup.context, setup.repo->directoryNode(), file);
        auto const& commands = compiler.commands();
        ASSERT_EQ(1, commands.size());
        auto command0 = *(commands.begin());

        ASSERT_EQ(1, command0->cmdInputs().size());
        auto input00 = command0->cmdInputs()[0];
        EXPECT_EQ(setup.mainFile, input00);
        ASSERT_EQ(rule->script.script, command0->script());
        ASSERT_EQ(1, command0->outputs().size());
        auto output00 = command0->outputs()[0];
        EXPECT_EQ(std::string("<repo>\\output\\main.obj"), output00->name().string());
    }


    TEST(BuildFileCompiler, emptyFile) {
        BuildFile::File file;
        ExecutionContext context;
        std::shared_ptr<DirectoryNode> baseDir;
        BuildFileCompiler compiler(&context, baseDir, file);
        EXPECT_EQ(0, compiler.commands().size());
    }

    TEST(BuildFileCompiler, scriptOnly) {
        BuildFile::File file;
        ExecutionContext context;
        std::shared_ptr<DirectoryNode> baseDir;
        
        auto rule = std::make_shared<BuildFile::Rule>();
        rule->forEach = false;
        BuildFile::Script script;
        script.script = "echo hello world";
        rule->script = script;
        file.variablesAndRules.push_back(rule);

        BuildFileCompiler compiler(&context, baseDir, file);
        EXPECT_EQ(1, compiler.commands().size());
        std::shared_ptr<CommandNode> cmd = *(compiler.commands().begin());
        EXPECT_EQ(script.script, cmd->script());
    }
}