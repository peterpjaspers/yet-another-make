#include "../Parser.h"
#include "gtest/gtest.h"
#include <string>

namespace {
    using namespace YAM;

    TEST(Parser, simpleRule) {
        const std::string file = R"(
        : 
            hello.c
            |>
                gcc hello.c -o hello
            |> 
            hello 
        )";
        Parser parser(file);

        auto buildFile = dynamic_pointer_cast<SyntaxTree::BuildFile>(parser.syntaxTree());
        ASSERT_NE(nullptr, buildFile);
        ASSERT_EQ(1, buildFile->children().size());
        auto rule = dynamic_pointer_cast<SyntaxTree::Rule>(buildFile->children()[0]);

        ASSERT_NE(nullptr, rule);
        ASSERT_EQ(3, rule->children().size());

        auto inputs = dynamic_pointer_cast<SyntaxTree::Inputs>(rule->children()[0]);
        ASSERT_NE(nullptr, inputs);
        ASSERT_EQ(1, inputs->children().size());
        auto input = dynamic_pointer_cast<SyntaxTree::Input>(inputs->children()[0]);
        ASSERT_NE(nullptr, input);
        EXPECT_FALSE(input->exclude);
        EXPECT_TRUE(input->glob.matches(std::string("hello.c")));

        auto script = dynamic_pointer_cast<SyntaxTree::Script>(rule->children()[1]);
        ASSERT_NE(nullptr, script);
        std::string expectedScript(R"(
                gcc hello.c -o hello
            )");
        EXPECT_EQ(expectedScript, script->script);

        auto outputs = dynamic_pointer_cast<SyntaxTree::Outputs>(rule->children()[2]);
        ASSERT_NE(nullptr, outputs);
        ASSERT_EQ(1, outputs->children().size());
        auto output = dynamic_pointer_cast<SyntaxTree::Output>(outputs->children()[0]);
        EXPECT_NE(nullptr, output);
        EXPECT_EQ("hello", output->path);
    }
}