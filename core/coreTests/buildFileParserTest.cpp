#include "../BuildFileParser.h"
#include "../Glob.h"
#include "gtest/gtest.h"
#include <string>

namespace {
    using namespace YAM;

    TEST(BuildFileParser, empty) {
        const std::string empty;

        BuildFileParser parser(empty);
        auto const buildFile = parser.file();
        EXPECT_EQ(0, buildFile->deps.depBuildFiles.size());
        EXPECT_EQ(0, buildFile->deps.depGlobs.size());
        EXPECT_EQ(0, buildFile->variablesAndRules.size());
    }
    TEST(BuildFileParser, depsAndRule) {
        const std::string rules = R"(
        buildfile ..\comp1\buildfile_yam.rb
        glob *.cpp
        buildfile ..\comp2\buildfile_yam.rb
        glob src\*.cpp

        : 
            hello.c
            |>
                gcc hello.c -o hello
            |> 
            %B.obj 
        )";
        BuildFileParser parser(rules);

        auto const buildFile = dynamic_pointer_cast<BuildFile::File>(parser.file());
        ASSERT_NE(nullptr, buildFile);
        ASSERT_EQ(2, buildFile->deps.depBuildFiles.size());
        EXPECT_EQ(R"(..\comp1\buildfile_yam.rb)", buildFile->deps.depBuildFiles[0]);
        EXPECT_EQ(R"(..\comp2\buildfile_yam.rb)", buildFile->deps.depBuildFiles[1]);
        ASSERT_EQ(2, buildFile->deps.depGlobs.size());
        EXPECT_EQ(R"(*.cpp)", buildFile->deps.depGlobs[0]);
        EXPECT_EQ(R"(src\*.cpp)", buildFile->deps.depGlobs[1]);

        ASSERT_EQ(1, buildFile->variablesAndRules.size());
        auto rule = dynamic_pointer_cast<BuildFile::Rule>(buildFile->variablesAndRules[0]);

        ASSERT_NE(nullptr, rule);

        ASSERT_EQ(1, rule->cmdInputs.inputs.size());
        auto const& input = rule->cmdInputs.inputs[0];
        EXPECT_FALSE(input.exclude);
        Glob glob(input.path);
        EXPECT_TRUE(glob.matches(std::string("hello.c")));

        std::string expectedScript(R"(
                gcc hello.c -o hello
            )");
        EXPECT_EQ(expectedScript, rule->script.script);

        ASSERT_EQ(1, rule->outputs.outputs.size());
        auto const& output = rule->outputs.outputs[0];
        EXPECT_EQ("%B.obj", output.path);
    }

    TEST(BuildFileParser, depBuildFile) {
        const std::string depOk = R"(buildfile ..\comp1\buildfile_yam.rb glob *.cpp)";

        BuildFileParser parser(depOk);
        auto const buildFile = parser.file();
        ASSERT_NE(nullptr, buildFile);
        EXPECT_EQ(1, buildFile->deps.depBuildFiles.size());
        EXPECT_EQ(R"(..\comp1\buildfile_yam.rb)", buildFile->deps.depBuildFiles[0]);
        EXPECT_EQ(1, buildFile->deps.depGlobs.size());
        EXPECT_EQ(R"(*.cpp)", buildFile->deps.depGlobs[0]);

        const std::string depWrong1 = R"(buildfiles ..\comp1\buildfile_yam.rb)";
        EXPECT_ANY_THROW(BuildFileParser parser(depWrong1););

        const std::string depWrong2 = R"(globs *.cpp)";
        EXPECT_ANY_THROW(BuildFileParser parser(depWrong2););
    }
    TEST(BuildFileParser, depsAndForeachRule) {
        const std::string rules = R"(
            buildfile ..\comp1\buildfile_yam.rb
            buildfile ..\comp2\**\
            glob *.cpp
            glob src\*.cpp
        : 
            foreach 
                "hello world.c" <someGroup>
                | hi.obj <..\modules\<someOtherGroup>
            |>
                gcc hello.c -o hello
            |> 
            %B.obj {objectsBin0} <objectsGroup0>
            %B.dep {objectsBin1} <objectsGroup1>
        )";
        BuildFileParser parser(rules);

        auto const buildFile = parser.file();
        ASSERT_NE(nullptr, buildFile);
        EXPECT_EQ(2, buildFile->deps.depBuildFiles.size());
        EXPECT_EQ(R"(..\comp1\buildfile_yam.rb)", buildFile->deps.depBuildFiles[0]);
        EXPECT_EQ(R"(..\comp2\**\)", buildFile->deps.depBuildFiles[1]);
        EXPECT_EQ(2, buildFile->deps.depGlobs.size());
        EXPECT_EQ(R"(*.cpp)", buildFile->deps.depGlobs[0]);
        EXPECT_EQ(R"(src\*.cpp)", buildFile->deps.depGlobs[1]);

        ASSERT_EQ(1, buildFile->variablesAndRules.size());
        auto rule = dynamic_pointer_cast<BuildFile::Rule>(buildFile->variablesAndRules[0]);

        ASSERT_NE(nullptr, rule);
        EXPECT_TRUE(rule->forEach);

        ASSERT_EQ(2, rule->cmdInputs.inputs.size());
        auto const& input0 = rule->cmdInputs.inputs[0];
        EXPECT_EQ(BuildFile::PathType::Path, input0.pathType);
        EXPECT_FALSE(input0.exclude);
        EXPECT_EQ("hello world.c", input0.path);

        auto const& input1 = rule->cmdInputs.inputs[1];
        EXPECT_FALSE(input1.exclude);
        EXPECT_EQ(BuildFile::PathType::Group, input1.pathType);
        EXPECT_TRUE(input1.pathType == BuildFile::PathType::Group);
        EXPECT_EQ("<someGroup>", input1.path);

        ASSERT_EQ(2, rule->orderOnlyInputs.inputs.size());
        auto const& ooinput0 = rule->orderOnlyInputs.inputs[0];
        EXPECT_FALSE(ooinput0.exclude);
        EXPECT_EQ(BuildFile::PathType::Path, ooinput0.pathType);
        EXPECT_EQ("hi.obj", ooinput0.path);
        auto const& ooinput1 = rule->orderOnlyInputs.inputs[1];
        EXPECT_FALSE(ooinput1.exclude);
        EXPECT_EQ(BuildFile::PathType::Group, ooinput1.pathType);
        EXPECT_EQ(R"(<..\modules\<someOtherGroup>)", ooinput1.path);

        std::string expectedScript(R"(
                gcc hello.c -o hello
            )");
        EXPECT_EQ(expectedScript, rule->script.script);

        ASSERT_EQ(2, rule->outputs.outputs.size());
        auto const& output0 = rule->outputs.outputs[0];
        EXPECT_EQ("%B.obj", output0.path);
        auto const& output1 = rule->outputs.outputs[1];
        EXPECT_EQ("%B.dep", output1.path);

        ASSERT_EQ(2, rule->outputGroups.size());
        auto const& group0 = rule->outputGroups[0];
        EXPECT_EQ("<objectsGroup0>", group0);
        auto const& group1 = rule->outputGroups[1];
        EXPECT_EQ("<objectsGroup1>", group1);

        ASSERT_EQ(2, rule->bins.size());
        auto const& bin0 = rule->bins[0];
        EXPECT_EQ("{objectsBin0}", bin0);
        auto const& bin1 = rule->bins[1];
        EXPECT_EQ("{objectsBin1}", bin1);
    }


    TEST(BuildFileParser, wrongScriptDelimitersToken) {
        const std::string file = R"(: hello.c >| gcc hello.c -o hello >| hello)";
        try
        {
            BuildFileParser parser(file);
        } catch (std::runtime_error e)
        {
            std::string expected(R"(Unexpected token at line 1, column 11 in file test
)");
            std::string actual = e.what();
            EXPECT_EQ(expected, actual);
        }
    }

    TEST(BuildFileParser, missingScriptDelimiterToken) {
        const std::string file = R"(: hello.c |> gcc hello.c -o hello hello)";
        try
        {
            BuildFileParser parser(file);
        } catch (std::runtime_error e)
        {
            std::string expected(R"(Unexpected token at line 1, column 11 in file test
)");
            std::string actual = e.what();
            EXPECT_EQ(expected, actual);
        }
    }


    TEST(BuildFileParser, illegalAbsoluteInputPath) {
        const std::string file = R"(: C:\hello.c |> gcc hello.c -o hello |> hello)";
        try
        {
            BuildFileParser parser(file);
        } catch (std::runtime_error e)
        {
            std::string expected("Illegal use of absolute path 'C:\\hello.c' at line 1, from column 3 to 13 in file test\n");
            std::string actual = e.what();
            EXPECT_EQ(expected, actual);
        }
    }

    TEST(BuildFileParser, illegalMissingEndQuoteInputPath) {
        const std::string file = R"(: "hello world |> gcc hello.c -o hello |> hello)";
        try
        {
            BuildFileParser parser(file);
        } catch (std::runtime_error e)
        {
            std::string expected("Missing endquote on input path at line 1, from column 3 to 48 in file test\n");
            std::string actual = e.what();
            EXPECT_EQ(expected, actual);
        }
    }

    TEST(BuildFileParser, illegalMissingEndQuoteOutputPath) {
        const std::string file = R"(: hello.c |> gcc hello.c -o hello |> "hello)";
        try
        {
            BuildFileParser parser(file);
        } catch (std::runtime_error e)
        {
            std::string expected("Missing endquote on output path at line 1, from column 38 to 44 in file test\n");
            std::string actual = e.what();
            EXPECT_EQ(expected, actual);
        }
    }
    TEST(BuildFileParser, illegalOutputPath) {
        const std::string file = R"(: hello.c |> gcc hello.c -o hello |> hello*)";
        try
        {
            BuildFileParser parser(file);
        } catch (std::runtime_error e)
        {
            std::string expected("Illegal use of glob characters in path 'hello*' at line 1, from column 38 to 44 in file test\n");
            std::string actual = e.what();
            EXPECT_EQ(expected, actual);
        }
    }

    TEST(BuildFileParser, twoRules) {
        const std::string file = R"(
: foreach *.dll |> echo %f > %o |> generated\%B.txt
: foreach *.dll |> echo %f > %o |> generated\%B.txt
)";
        BuildFileParser parser(file, "test");
        auto const buildFile = parser.file();
        ASSERT_NE(nullptr, buildFile);
        EXPECT_EQ(0, buildFile->deps.depBuildFiles.size());
        EXPECT_EQ(0, buildFile->deps.depGlobs.size());
        ASSERT_EQ(2, buildFile->variablesAndRules.size());
    }
}