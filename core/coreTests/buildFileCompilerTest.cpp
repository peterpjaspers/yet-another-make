#include "../BuildFileCompiler.h"
#include "../FileRepository.h"
#include "../DirectoryNode.h"
#include "../SourceFileNode.h"
#include "../CommandNode.h"
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
        std::shared_ptr<SourceFileNode> libFile;

        CompilerSetup()
            : repoName("repo")
            , repoDir(FileSystem::createUniqueDirectory())
            , testTree(repoDir, 2, RegexSet({ }))
        {
            //context.threadPool().size(1);
            std::filesystem::create_directory(repoDir / "src");
            std::filesystem::path f1(repoDir / "src\\main.cpp");
            std::filesystem::path f2(repoDir / "src\\lib.cpp");
            std::ofstream s1(f1.string().c_str());
            s1 << "void main() {}" << std::endl;
            s1.close();
            std::ofstream s2(f2.string().c_str());
            s2 << "void lib() {}" << std::endl;
            s2.close();
            repo = std::make_shared<FileRepository>(
                "repo",
                repoDir,
                &context);
            context.addRepository(repo);
            bool completed = YAMTest::executeNode(repo->directoryNode().get());
            EXPECT_TRUE(completed);
            mainFile = dynamic_pointer_cast<SourceFileNode>(context.nodes().find(repo->symbolicPathOf(f1)));
            libFile = dynamic_pointer_cast<SourceFileNode>(context.nodes().find(repo->symbolicPathOf(f2)));
            EXPECT_NE(nullptr, mainFile);
            EXPECT_NE(nullptr, libFile);
        }
    };


    TEST(BuildFileCompiler, foreachGlobInput) {
        CompilerSetup setup;
        BuildFile::Input input;
        input.exclude = false;
        input.pathPattern = "src\\*.cpp";
        auto rule = std::make_shared<BuildFile::Rule>();
        rule->forEach = true;
        rule->cmdInputs.inputs.push_back(input);
        rule->script.script = "echo hello world";
        BuildFile::File file;
        file.variablesAndRules.push_back(rule);

        BuildFileCompiler compiler(&setup.context, setup.repo->directoryNode(), file);
        auto const& commands = compiler.commands();
        ASSERT_EQ(2, commands.size());

        ASSERT_EQ(1, commands[0]->cmdInputs().size());
        auto input0 = commands[0]->cmdInputs()[0];
        EXPECT_EQ(setup.libFile, input0);
        ASSERT_EQ(rule->script.script, commands[0]->script());

        ASSERT_EQ(1, commands[1]->cmdInputs().size());
        auto input1 = commands[1]->cmdInputs()[0];
        EXPECT_EQ(setup.mainFile, input1);
        ASSERT_EQ(rule->script.script, commands[1]->script());
    }


    TEST(BuildFileCompiler, emptyFile) {
        BuildFile::File file;
        ExecutionContext context;
        std::shared_ptr<DirectoryNode> baseDir;
        BuildFileCompiler compiler(&context, baseDir, file);
        EXPECT_EQ(0, compiler.commands().size());
    }

    TEST(BuildFileCompiler, noInputsAndOutputs) {
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
        std::shared_ptr<CommandNode> cmd = compiler.commands()[0];
        EXPECT_EQ(script.script, cmd->script());
    }
}