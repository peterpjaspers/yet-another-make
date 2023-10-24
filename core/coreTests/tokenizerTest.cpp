#include "../Tokenizer.h"
#include "gtest/gtest.h"

namespace {
    using namespace YAM;

    std::vector<TokenSpec> specs = {
        TokenSpec(R"(^\s+)", "skip"), // whitespace
        TokenSpec(R"(^\/\/.*)", "skip"), // single-line comment
        TokenSpec(R"(^\/\*[\s\S]*?\*\/)", "skip"), // multi-line comment
        TokenSpec(R"(^:)", "rule"),
        TokenSpec(R"(^\^)", "not"),
        TokenSpec(R"(^\\?(?:[\w\.\*\?\[\]-])+(?:\\([\w\.\*\?\[\]-])+)*)", "glob"),

        // TODO: only matches single line commands
        TokenSpec(R"(^\|>\s+(.*)\s+\|>)", "command", 1),

        // TODO: this multiline command regex works in https://regex101.com/.
        // But here it results in a hangup in regex_search
        //TokenSpec(R"(^\|>\s+((?:.*\n?)+)\s+\|>)", "command", 1),
    };

    TEST(Tokenizer, whiteSpace) {
        const std::string whitespace(R"(    )");
        Tokenizer tokenizer(whitespace, specs);
        Token token;
        tokenizer.readNextToken(token);
        EXPECT_EQ("eos", token.type);
        EXPECT_EQ("", token.value);
    }

    TEST(Tokenizer, ruleStart) {
        const std::string rule(R"(
  : 
  )");
        Tokenizer tokenizer(rule, specs);
        Token token;
        tokenizer.readNextToken(token);
        EXPECT_EQ("rule", token.type);
        EXPECT_EQ(":", token.value);
        EXPECT_EQ(3, tokenizer.tokenStartOffset());
        EXPECT_EQ(4, tokenizer.tokenEndOffset());
        EXPECT_EQ(1, tokenizer.tokenStartLine());
        EXPECT_EQ(1, tokenizer.tokenEndLine());
        EXPECT_EQ(2, tokenizer.tokenStartColumn());
        EXPECT_EQ(3, tokenizer.tokenEndColumn());
        EXPECT_EQ(4, tokenizer.cursor());
        EXPECT_EQ(1, tokenizer.lineBeginOffset());
        EXPECT_EQ(3, tokenizer.column());
        tokenizer.readNextToken(token);
        EXPECT_EQ("eos", token.type);
        EXPECT_EQ("", token.value);
    }

    TEST(Tokenizer, commentLine) {
        const std::string commentLine(R"(  : // comment  :  
  : )");
        Tokenizer tokenizer(commentLine, specs);
        Token token;
        tokenizer.readNextToken(token);
        EXPECT_EQ("rule", token.type);
        EXPECT_EQ(":", token.value);
        tokenizer.readNextToken(token);
        EXPECT_EQ("rule", token.type);
        EXPECT_EQ(":", token.value);
        EXPECT_EQ(22, tokenizer.tokenStartOffset());
        EXPECT_EQ(23, tokenizer.tokenEndOffset());
        EXPECT_EQ(1, tokenizer.tokenStartLine());
        EXPECT_EQ(1, tokenizer.tokenEndLine());
        EXPECT_EQ(2, tokenizer.tokenStartColumn());
        EXPECT_EQ(3, tokenizer.tokenEndColumn());
        EXPECT_EQ(23, tokenizer.cursor());
        EXPECT_EQ(20, tokenizer.lineBeginOffset());
        EXPECT_EQ(3, tokenizer.column());
        tokenizer.readNextToken(token);
        EXPECT_EQ("eos", token.type);
        EXPECT_EQ("", token.value);
    }

    TEST(Tokenizer, commentLines) {
        const std::string commentLines(R"(         
        /* c1
         * c2
        c3
        c4 
  */  :)");
        Tokenizer tokenizer(commentLines, specs);
        Token token;
        tokenizer.readNextToken(token);
        EXPECT_EQ("rule", token.type);
        EXPECT_EQ(":", token.value);
        EXPECT_EQ(67, tokenizer.tokenStartOffset());
        EXPECT_EQ(68, tokenizer.tokenEndOffset());
        EXPECT_EQ(5, tokenizer.tokenStartLine());
        EXPECT_EQ(5, tokenizer.tokenEndLine());
        EXPECT_EQ(6, tokenizer.tokenStartColumn());
        EXPECT_EQ(7, tokenizer.tokenEndColumn());
        EXPECT_EQ(68, tokenizer.cursor());
        EXPECT_EQ(61, tokenizer.lineBeginOffset());
        EXPECT_EQ(7, tokenizer.column());
        tokenizer.readNextToken(token);
        EXPECT_EQ("eos", token.type);
        EXPECT_EQ("", token.value);
    }

    TEST(Tokenizer, not) {
        const std::string whitespace(R"(^aap.c)");
        Tokenizer tokenizer(whitespace, specs);
        Token token;
        tokenizer.readNextToken(token);
        EXPECT_EQ("not", token.type);
        EXPECT_EQ("^", token.value);
        tokenizer.readNextToken(token);
        EXPECT_EQ("glob", token.type);
        EXPECT_EQ("aap.c", token.value);
        tokenizer.readNextToken(token);
        EXPECT_EQ("eos", token.type);
        EXPECT_EQ("", token.value);
    }

    TEST(Tokenizer, relativePath3) {
        const std::string path(R"(aap\noot\mies.txt)");
        Tokenizer tokenizer(path, specs);
        Token token;
        tokenizer.readNextToken(token);
        EXPECT_EQ("glob", token.type);
        EXPECT_EQ(path, token.value);
        tokenizer.readNextToken(token);
        EXPECT_EQ("eos", token.type);
        EXPECT_EQ("", token.value);
    }

    TEST(Tokenizer, relativePath1) {
        const std::string path(R"(mies.txt)");
        Tokenizer tokenizer(path, specs);
        Token token;
        tokenizer.readNextToken(token);
        EXPECT_EQ("glob", token.type);
        EXPECT_EQ(path, token.value);
        tokenizer.readNextToken(token);
        EXPECT_EQ("eos", token.type);
        EXPECT_EQ("", token.value);
    }

    TEST(Tokenizer, absolutePath3) {
        const std::string path(R"(\aap\noot\mies.txt)");
        Tokenizer tokenizer(path, specs);
        Token token;
        tokenizer.readNextToken(token);
        EXPECT_EQ("glob", token.type);
        EXPECT_EQ(path, token.value);
        tokenizer.readNextToken(token);
        EXPECT_EQ("eos", token.type);
        EXPECT_EQ("", token.value);
    }

    TEST(Tokenizer, absolutePath1) {
        const std::string path(R"(mies.txt)");
        Tokenizer tokenizer(path, specs);
        Token token;
        tokenizer.readNextToken(token);
        EXPECT_EQ("glob", token.type);
        EXPECT_EQ(path, token.value);
        tokenizer.readNextToken(token);
        EXPECT_EQ("eos", token.type);
        EXPECT_EQ("", token.value);
    }

    TEST(Tokenizer, relativeGlob1) {
        const std::string glob(R"(aap\a?b?[cde]*.txt)");
        Tokenizer tokenizer(glob, specs);
        Token token;
        tokenizer.readNextToken(token);
        EXPECT_EQ("glob", token.type);
        EXPECT_EQ(glob, token.value);
        tokenizer.readNextToken(token);
        EXPECT_EQ("eos", token.type);
        EXPECT_EQ("", token.value);
    }

    TEST(Tokenizer, absoluteGlob1) {
        const std::string glob(R"(\aap\a?b?[cde]*.txt)");
        Tokenizer tokenizer(glob, specs);
        Token token;
        tokenizer.readNextToken(token);
        EXPECT_EQ("glob", token.type);
        EXPECT_EQ(glob, token.value);
        tokenizer.readNextToken(token);
        EXPECT_EQ("eos", token.type);
        EXPECT_EQ("", token.value);
    }

    TEST(Tokenizer, rule) {
        const std::string rule(R"(: src\hello.c |> gcc src\hello.c -o bin\hello |> bin\hello )");
        Tokenizer tokenizer(rule, specs);
        Token token;
        tokenizer.readNextToken(token);
        EXPECT_EQ("rule", token.type);
        EXPECT_EQ(":", token.value);
        tokenizer.readNextToken(token);
        EXPECT_EQ("glob", token.type);
        EXPECT_EQ("src\\hello.c", token.value);
        tokenizer.readNextToken(token);
        EXPECT_EQ("command", token.type);
        EXPECT_EQ("gcc src\\hello.c -o bin\\hello", token.value);
        tokenizer.readNextToken(token);
        EXPECT_EQ("glob", token.type);
        EXPECT_EQ("bin\\hello", token.value);
        tokenizer.readNextToken(token);
        EXPECT_EQ("eos", token.type);
        EXPECT_EQ("", token.value);
    }
}