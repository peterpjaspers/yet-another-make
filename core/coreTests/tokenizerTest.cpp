#include "../Tokenizer.h"
#include "gtest/gtest.h"

namespace {
    using namespace YAM;

    std::vector<TokenSpec> specs = {
        TokenSpec(R"(^\s+)", "skip"), // whitespace
        TokenSpec(R"(^\/\/.*)", "skip"), // single-line comment
        TokenSpec(R"(^\/\*[\s\S]*?\*\/)", "skip"), // multi-line comment
        TokenSpec(R"(^:)", "rule"),
        //TokenSpec(R"(^([A-Za-z]:)?([\w\.-]+)?(\\[\w\.-]+)*)", "path"), // with drive letter
        TokenSpec(R"(^[\\]?(?:[\w\.-]+)(?:\\[\w\.-]+)*)", "path"),
        //TokenSpec(R"(^([A-Za-z]:)?([\w\.\*\?\[\]-]+)?(\\[\w\.\*\?\[\]-]+)*)", "glob"), //with drive letter
        TokenSpec(R"(^[\\]?(?:[\w\.\*\?\[\]-]+)(?:\\[\w\.\*\?\[\]-]+)*)", "glob"),
        TokenSpec(R"(^\|>.*\|>)", "command"),
    };

    
    std::string whitespace(R"(    //)");
    //std::string commentLines(R"(   
    ///* c1
    // * c2
    //c3
    //c4 
    //*/)");
    // 

    TEST(Tokenizer, whiteSpace) {
        const std::string whitespace(R"(    )");
        Tokenizer tokenizer(whitespace, specs);
        Token token;
        tokenizer.readNextToken(token);
        EXPECT_EQ("eos", token.type);
        EXPECT_EQ("", token.value);
    }

    TEST(Tokenizer, rule) {
        const std::string rule(R"(  : )");
        Tokenizer tokenizer(rule, specs);
        Token token;
        tokenizer.readNextToken(token);
        EXPECT_EQ("rule", token.type);
        EXPECT_EQ(":", token.value);
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
        tokenizer.readNextToken(token);
        EXPECT_EQ("eos", token.type);
        EXPECT_EQ("", token.value);
    }

    TEST(Tokenizer, relativePath3) {
        const std::string path(R"(aap\noot\mies.txt)");
        Tokenizer tokenizer(path, specs);
        Token token;
        tokenizer.readNextToken(token);
        EXPECT_EQ("path", token.type);
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
        EXPECT_EQ("path", token.type);
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
        EXPECT_EQ("path", token.type);
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
        EXPECT_EQ("path", token.type);
        EXPECT_EQ(path, token.value);
        tokenizer.readNextToken(token);
        EXPECT_EQ("eos", token.type);
        EXPECT_EQ("", token.value);
    }

    TEST(Tokenizer, relativeGlob1) {
        const std::string glob(R"(*.txt)");
        Tokenizer tokenizer(glob, specs);
        Token token;
        tokenizer.readNextToken(token);
        EXPECT_EQ("glob", token.type);
        EXPECT_EQ(glob, token.value);
        tokenizer.readNextToken(token);
        EXPECT_EQ("eos", token.type);
        EXPECT_EQ("", token.value);
    }
}