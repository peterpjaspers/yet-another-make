#include "../Tokenizer.h"
#include "../tokenSpecs.h" // defines variable tokenSpecs
#include "gtest/gtest.h"
#include <regex>
#include <chrono>

namespace {
    using namespace YAM;

    bool testCommandMatching() {
        const std::string group(R"(
                gcc |
                src\hello.c > piet
               -o bin\hello 
            )");

        // This regex executes very slowly, probably because of excessive
        // backtracking. See https://www.regular-expressions.info/catastrophic.html.
        //const std::regex cmdRe(R"(^\|>((?:.*\s*)*)\|>)");

        const std::regex cmdRe(R"(^\|>(((?!\|>).|\s)*)\|>)");

        bool allMatched = true;
        {
            std::cmatch match;
            const std::string command(R"(|>
                gcc |
                src\hello.c > piet
               -o bin\hello 
            |>)");
            auto start = std::chrono::system_clock::now();
            bool matched = std::regex_search(command.c_str(), match, cmdRe, std::regex_constants::match_continuous);
            auto end = std::chrono::system_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            allMatched = allMatched && group == (matched ? match[1] : std::string(""));
        }
        {
            std::string group(R"(gcc src\hello.c -o bin\hello )");
            std::cmatch match;
            const std::string command(R"(|>gcc src\hello.c -o bin\hello |>)");
            auto start = std::chrono::system_clock::now();
            bool matched = std::regex_search(command.c_str(), match, cmdRe, std::regex_constants::match_continuous);
            auto end = std::chrono::system_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            allMatched = allMatched && group == (matched ? match[1] : std::string(""));
        }
        {
            std::cmatch match;
            const std::string command(R"(|>
                gcc |
                src\hello.c > piet
               -o bin\hello 
            |> bin\hello)");
            auto start = std::chrono::system_clock::now();
            bool matched = std::regex_search(command.c_str(), match, cmdRe, std::regex_constants::match_continuous);
            auto end = std::chrono::system_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            allMatched = allMatched && group == (matched ? match[1] : std::string(""));
        }
        {
            std::string group(R"(gcc | src\hello.c -o > bin\hello )");
            std::cmatch match;
            const std::string command(R"(|>gcc | src\hello.c -o > bin\hello |> bin\hello)");
            auto start = std::chrono::system_clock::now();
            bool matched = std::regex_search(command.c_str(), match, cmdRe, std::regex_constants::match_continuous);
            auto end = std::chrono::system_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            allMatched = allMatched && group == (matched ? match[1] : std::string(""));
        }
        return allMatched;
    }

    TEST(Tokenizer, performance) {
        EXPECT_TRUE(testCommandMatching());
    }

    TEST(Tokenizer, whiteSpace) {
        const std::string whitespace(R"(    )");
        Tokenizer tokenizer(whitespace, tokenSpecs);
        Token token;
        tokenizer.readNextToken(token);
        EXPECT_EQ("eos", token.type);
        EXPECT_EQ("", token.value);
    }

    TEST(Tokenizer, ruleStart) {
        const std::string rule(R"(
  : 
  )");
        Tokenizer tokenizer(rule, tokenSpecs);
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
        Tokenizer tokenizer(commentLine, tokenSpecs);
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
        Tokenizer tokenizer(commentLines, tokenSpecs);
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
        Tokenizer tokenizer(whitespace, tokenSpecs);
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
        Tokenizer tokenizer(path, tokenSpecs);
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
        Tokenizer tokenizer(path, tokenSpecs);
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
        Tokenizer tokenizer(path, tokenSpecs);
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
        Tokenizer tokenizer(path, tokenSpecs);
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
        Tokenizer tokenizer(glob, tokenSpecs);
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
        Tokenizer tokenizer(glob, tokenSpecs);
        Token token;
        tokenizer.readNextToken(token);
        EXPECT_EQ("glob", token.type);
        EXPECT_EQ(glob, token.value);
        tokenizer.readNextToken(token);
        EXPECT_EQ("eos", token.type);
        EXPECT_EQ("", token.value);
    }

    TEST(Tokenizer, singleLineScript) {
        const std::string group(R"(gcc src\hello.c -o bin\hello)");
        const std::string script = R"(|>gcc src\hello.c -o bin\hello|> )";

        Tokenizer tokenizer(script, tokenSpecs);
        Token token;

        tokenizer.readNextToken(token);
        EXPECT_EQ("script", token.type);
        EXPECT_EQ(group, token.value);
        tokenizer.readNextToken(token);
        EXPECT_EQ("eos", token.type);
        EXPECT_EQ("", token.value);
    }

    TEST(Tokenizer, multiLineScript) {
        const std::string group(R"(
                gcc 
                src\hello.c 
               -o bin\hello)");
        const std::string script = R"(|>
                gcc 
                src\hello.c 
               -o bin\hello|> )";

        Tokenizer tokenizer(script, tokenSpecs);
        Token token;

        tokenizer.readNextToken(token);
        EXPECT_EQ("script", token.type);
        EXPECT_EQ(group, token.value);
        tokenizer.readNextToken(token);
        EXPECT_EQ("eos", token.type);
        EXPECT_EQ("", token.value);
    }
    TEST(Tokenizer, rule) {
        const std::string command(R"(
                gcc 
                src\hello.c 
               -o bin\hello 
            )");
        const std::string rule(R"(: 
            src\hello.c |>
                gcc 
                src\hello.c 
               -o bin\hello 
            |> bin\hello )");

        Tokenizer tokenizer(rule, tokenSpecs);
        Token token;

        tokenizer.readNextToken(token);
        EXPECT_EQ("rule", token.type);
        EXPECT_EQ(":", token.value);
        tokenizer.readNextToken(token);
        EXPECT_EQ("glob", token.type);
        EXPECT_EQ("src\\hello.c", token.value);
        tokenizer.readNextToken(token);
        //EXPECT_EQ("command", token.type);
        //EXPECT_EQ(command, token.value);
        //tokenizer.readNextToken(token);
        //EXPECT_EQ("glob", token.type);
        //EXPECT_EQ("bin\\hello", token.value);
        //tokenizer.readNextToken(token);
        //EXPECT_EQ("eos", token.type);
        //EXPECT_EQ("", token.value);
    }
}