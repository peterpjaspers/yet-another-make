#include "../BuildFileParser.h"
#include "../Glob.h"
#include "gtest/gtest.h"
#include <string>

namespace {
    using namespace YAM;

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
        Glob glob(input.pathPattern);
        EXPECT_TRUE(glob.matches(std::string("hello.c")));

        std::string expectedScript(R"(
                gcc hello.c -o hello
            )");
        EXPECT_EQ(expectedScript, rule->script.script);

        ASSERT_EQ(1, rule->outputs.outputs.size());
        auto const& output = rule->outputs.outputs[0];
        EXPECT_EQ("%B.obj", output.path);
    }

    TEST(BuildFileParser, depsAndForeachRule) {
        const std::string rules = R"(
            buildfile ..\comp1\buildfile_yam.rb
            buildfile ..\comp2\buildfile_yam.rb
            glob *.cpp
            glob src\*.cpp
        : 
            foreach 
                hello.c <someGroup>
                | hi.obj <..\modules\<someOtherGroup>
            |>
                gcc hello.c -o hello
            |> 
            %B.obj <objects>
        )";
        BuildFileParser parser(rules);

        auto const buildFile = parser.file();
        ASSERT_NE(nullptr, buildFile);
        EXPECT_EQ(2, buildFile->deps.depBuildFiles.size());
        EXPECT_EQ(R"(..\comp1\buildfile_yam.rb)", buildFile->deps.depBuildFiles[0]);
        EXPECT_EQ(R"(..\comp2\buildfile_yam.rb)", buildFile->deps.depBuildFiles[1]);
        EXPECT_EQ(2, buildFile->deps.depGlobs.size());
        EXPECT_EQ(R"(*.cpp)", buildFile->deps.depGlobs[0]);
        EXPECT_EQ(R"(src\*.cpp)", buildFile->deps.depGlobs[1]);

        ASSERT_EQ(1, buildFile->variablesAndRules.size());
        auto rule = dynamic_pointer_cast<BuildFile::Rule>(buildFile->variablesAndRules[0]);

        ASSERT_NE(nullptr, rule);
        EXPECT_TRUE(rule->forEach);
        EXPECT_EQ("<objects>", rule->outputGroup);
        ASSERT_EQ(2, rule->cmdInputs.inputs.size());

        auto const& input0 = rule->cmdInputs.inputs[0];
        EXPECT_FALSE(input0.exclude);
        EXPECT_EQ("hello.c", input0.pathPattern);

        auto const& input1 = rule->cmdInputs.inputs[1];
        EXPECT_FALSE(input1.exclude);
        EXPECT_TRUE(input1.isGroup);
        EXPECT_EQ("<someGroup>", input1.pathPattern);

        ASSERT_EQ(2, rule->orderOnlyInputs.inputs.size());
        auto const& ooinput0 = rule->orderOnlyInputs.inputs[0];
        EXPECT_FALSE(ooinput0.exclude);
        EXPECT_FALSE(ooinput0.isGroup);
        EXPECT_EQ("hi.obj", ooinput0.pathPattern);
        auto const& ooinput1 = rule->orderOnlyInputs.inputs[1];
        EXPECT_FALSE(ooinput1.exclude);
        EXPECT_TRUE(ooinput1.isGroup);
        EXPECT_EQ(R"(<..\modules\<someOtherGroup>)", ooinput1.pathPattern);

        std::string expectedScript(R"(
                gcc hello.c -o hello
            )");
        EXPECT_EQ(expectedScript, rule->script.script);

        ASSERT_EQ(1, rule->outputs.outputs.size());
        auto const& output = rule->outputs.outputs[0];
        EXPECT_EQ("%B.obj", output.path);
    }


    TEST(BuildFileParser, wrongScriptDelimitersToken) {
        const std::string file = R"(: hello.c >| gcc hello.c -o hello >| hello)";
        try
        {
            BuildFileParser parser(file);
        } catch (std::runtime_error e)
        {
            std::string expected(R"(Unexpected token at line 0, column 10 in file test
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
            std::string expected(R"(Unexpected token at line 0, column 10 in file test
)");
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
            std::string expected("Path 'hello*' is not allowed to contain glob special characters\nAt line 0, from column 37 to 43\n");
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