#include "../BuildFileParser.h"
#include "../Glob.h"
#include "gtest/gtest.h"
#include <string>

namespace {
    using namespace YAM;

    TEST(BuildFileParser, simpleRule) {
        const std::string rules = R"(
        : 
            hello.c
            |>
                gcc hello.c -o hello
            |> 
            hello 
        )";
        BuildFileParser parser(rules);

        auto const buildFile = dynamic_pointer_cast<BuildFile::File>(parser.file());
        ASSERT_NE(nullptr, buildFile);
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
        EXPECT_EQ("hello", output.path);
    }

    TEST(BuildFileParser, wrongScriptDelimitersToken) {
        const std::string file = R"(: hello.c >| gcc hello.c -o hello >| hello)";
        try
        {
            BuildFileParser parser(file);
        } catch (std::runtime_error e)
        {
            std::string expected(R"(Unexpected token at line 0, column 10
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
            std::string expected(R"(Unexpected token at line 0, column 10
)");
            std::string actual = e.what();
            EXPECT_EQ(expected, actual);
        }
    }
}