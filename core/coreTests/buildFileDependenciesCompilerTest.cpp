#include "../BuildFileDependenciesCompiler.h"
#include "../FileRepository.h"
#include "../DirectoryNode.h"
#include "../BuildFileCompilerNode.h"
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

    class TestSetup {
    public:
        DirectoryTree repoTree;
        ExecutionContext context;
        std::shared_ptr<FileRepository> fileRepo;
        std::shared_ptr<BuildFileCompilerNode> bfcn1;
        std::shared_ptr<BuildFileCompilerNode> bfcn2;

        TestSetup()
            : repoTree(FileSystem::createUniqueDirectory("_buildFileDepenciesCompilerTest"), 1, RegexSet())
            , fileRepo(std::make_shared<FileRepository>("repo", repoTree.path(), &context))
        {
            std::filesystem::create_directory(repoTree.path() / "src1");
            std::filesystem::create_directory(repoTree.path() / "src2");

            std::filesystem::path bf1Path(repoTree.path() / "src1\\buildfile_yam.bat");
            std::ofstream bf1(bf1Path.string().c_str());
            EXPECT_TRUE(bf1.is_open());
            bf1.close();
            std::filesystem::path bf2Path(repoTree.path() / "src2\\buildfile_yam.bat");
            std::ofstream bf2(bf2Path.string().c_str());
            EXPECT_TRUE(bf2.is_open());
            bf2.close();

            context.addRepository(fileRepo);
            auto dirNode = fileRepo->directoryNode();
            bool completed = YAMTest::executeNode(dirNode.get());
            EXPECT_TRUE(completed);
            auto bfcn1Dir = dynamic_pointer_cast<DirectoryNode>(context.nodes().find(dirNode->name() / "src1"));
            auto bfcn2Dir = dynamic_pointer_cast<DirectoryNode>(context.nodes().find(dirNode->name() / "src2"));
            bfcn1 = bfcn1Dir->buildFileCompilerNode();
            bfcn2 = bfcn2Dir->buildFileCompilerNode();
        }
    };


    TEST(BuildFileDependenciesCompiler, twoBFPNsAndThreeGlobs) {
        TestSetup setup;
        BuildFile::Input input1;
        input1.exclude = false;
        input1.pathType = BuildFile::PathType::Glob;
        input1.path = "src1\\*.cpp";
        BuildFile::Input input2;
        input2.exclude = false;
        input2.pathType = BuildFile::PathType::Glob;
        input2.path = "src2\\*.cpp";
        auto rule = std::make_shared<BuildFile::Rule>();
        rule->forEach = true;
        rule->cmdInputs.inputs.push_back(input1);
        rule->cmdInputs.inputs.push_back(input2);
        BuildFile::File file;
        file.deps.depBuildFiles.push_back(setup.repoTree.path() / "src1");
        file.deps.depBuildFiles.push_back(setup.repoTree.path() / "src2");
        file.deps.depGlobs.push_back("*.h");
        file.variablesAndRules.push_back(rule);

        std::filesystem::path globNameSpace("private");
        BuildFileDependenciesCompiler compiler(&setup.context, setup.fileRepo->directoryNode(), file, globNameSpace);

        auto const& bfcns = compiler.compilers();
        ASSERT_EQ(2, bfcns.size());
        auto const& bfcn1It = bfcns.find(setup.bfcn1->name());
        auto const& bfcn2It = bfcns.find(setup.bfcn2->name());
        ASSERT_NE(bfcns.end(), bfcn1It);
        ASSERT_NE(bfcns.end(), bfcn1It);
        EXPECT_EQ(setup.bfcn1, bfcn1It->second);
        EXPECT_EQ(setup.bfcn2, bfcn2It->second);

        auto const& globs = compiler.globs();
        ASSERT_EQ(3, globs.size());
        std::filesystem::path glob1Name(globNameSpace / setup.fileRepo->directoryNode()->name() / "*.h");
        std::filesystem::path glob2Name(globNameSpace / setup.fileRepo->directoryNode()->name() / "src1\\*.cpp");
        std::filesystem::path glob3Name(globNameSpace / setup.fileRepo->directoryNode()->name() / "src2\\*.cpp");
        auto const& glob1It = globs.find(glob1Name);
        auto const& glob2It = globs.find(glob2Name);
        auto const& glob3It = globs.find(glob3Name);
        ASSERT_NE(globs.end(), glob1It);
        ASSERT_NE(globs.end(), glob2It);
        ASSERT_NE(globs.end(), glob3It);
    }
}