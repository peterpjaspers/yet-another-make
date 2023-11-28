#include "executeNode.h"
#include "DirectoryTree.h"
#include "../BuildFile.h"
#include "../BuildFileCompiler.h"
#include "../FileRepository.h"
#include "../DirectoryNode.h"
#include "../SourceFileNode.h"
#include "../CommandNode.h"
#include "../ExecutionContext.h"

#include "gtest/gtest.h"
#include <chrono>

namespace
{
    using namespace YAM;

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