#include "../BuildFileTokenizer.h"
#include "../BuildFileTokenSpecs.h" // defines variable tokenSpecs
#include "gtest/gtest.h"
#include <regex>
#include <chrono>

namespace {
    using namespace YAM;

    ITokenSpec const* whiteSpace(BuildFileTokenSpecs::whiteSpace());
    ITokenSpec const* comment1(BuildFileTokenSpecs::comment1());
    ITokenSpec const* commentN(BuildFileTokenSpecs::commentN());
    ITokenSpec const* depBuildFile(BuildFileTokenSpecs::depBuildFile());
    ITokenSpec const* depGlob(BuildFileTokenSpecs::depGlob());
    ITokenSpec const* rule(BuildFileTokenSpecs::rule());
    ITokenSpec const* foreach(BuildFileTokenSpecs::foreach());
    ITokenSpec const* ignore(BuildFileTokenSpecs::ignore());
    ITokenSpec const* curlyOpen(BuildFileTokenSpecs::curlyOpen());
    ITokenSpec const* curlyClose(BuildFileTokenSpecs::curlyClose());
    ITokenSpec const* cmdStart(BuildFileTokenSpecs::cmdStart());
    ITokenSpec const* cmdEnd(BuildFileTokenSpecs::cmdEnd());
    ITokenSpec const* script(BuildFileTokenSpecs::script());
    ITokenSpec const* vertical(BuildFileTokenSpecs::vertical());
    ITokenSpec const* glob(BuildFileTokenSpecs::glob());

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

    TEST(BuildFileTokenizer, performance) {
        EXPECT_TRUE(testCommandMatching());
    }

    TEST(BuildFileTokenizer, whiteSpace) {
        const std::string ws(R"(    )");
        BuildFileTokenizer tokenizer("testFile", ws);
        Token token;
        token = tokenizer.readNextToken({ whiteSpace });
        EXPECT_EQ(whiteSpace, token.spec);
        token = tokenizer.readNextToken({whiteSpace});
        EXPECT_EQ(tokenizer.eosTokenSpec(), token.spec);
    }

    TEST(BuildFileTokenizer, ruleStart) {
        const std::string ruleStr(R"(
  : 
  )");
        BuildFileTokenizer tokenizer("testFile", ruleStr);
        Token token;
        tokenizer.skip({ whiteSpace });
        token = tokenizer.readNextToken({ rule });
        EXPECT_EQ(rule, token.spec);
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
        tokenizer.skip({ whiteSpace });
        token = tokenizer.readNextToken({whiteSpace});
        EXPECT_EQ(tokenizer.eosTokenSpec(), token.spec);
    }

    TEST(BuildFileTokenizer, commentLine) {
        const std::string commentLine(R"(  : // comment  :  
  ://  comment
 )");
        BuildFileTokenizer tokenizer("testFile", commentLine);
        Token token;
        token = tokenizer.readNextToken({ whiteSpace });
        token = tokenizer.readNextToken({ rule });
        EXPECT_EQ(rule, token.spec);
        EXPECT_EQ(":", token.value);
        EXPECT_EQ(2, tokenizer.tokenStartOffset());
        EXPECT_EQ(3, tokenizer.tokenEndOffset());
        EXPECT_EQ(0, tokenizer.tokenStartLine());
        EXPECT_EQ(0, tokenizer.tokenEndLine());
        EXPECT_EQ(2, tokenizer.tokenStartColumn());
        EXPECT_EQ(3, tokenizer.tokenEndColumn());
        EXPECT_EQ(3, tokenizer.cursor());
        EXPECT_EQ(0, tokenizer.lineBeginOffset());
        EXPECT_EQ(3, tokenizer.column());
    
        token = tokenizer.readNextToken({ whiteSpace });
        token = tokenizer.readNextToken({ comment1 });
        EXPECT_EQ(comment1, token.spec);
        EXPECT_EQ(4, tokenizer.tokenStartOffset());
        EXPECT_EQ(19, tokenizer.tokenEndOffset());
        EXPECT_EQ(0, tokenizer.tokenStartLine());
        EXPECT_EQ(0, tokenizer.tokenEndLine());
        EXPECT_EQ(4, tokenizer.tokenStartColumn());
        EXPECT_EQ(19, tokenizer.tokenEndColumn());
        EXPECT_EQ(19, tokenizer.cursor());
        EXPECT_EQ(0, tokenizer.lineBeginOffset());
        EXPECT_EQ(19, tokenizer.column());

        token = tokenizer.readNextToken({ whiteSpace });
        EXPECT_EQ(whiteSpace, token.spec);
        EXPECT_EQ(19, tokenizer.tokenStartOffset());
        EXPECT_EQ(22, tokenizer.tokenEndOffset());
        EXPECT_EQ(0, tokenizer.tokenStartLine());
        EXPECT_EQ(1, tokenizer.tokenEndLine());
        EXPECT_EQ(19, tokenizer.tokenStartColumn());
        EXPECT_EQ(2, tokenizer.tokenEndColumn());
        EXPECT_EQ(22, tokenizer.cursor());
        EXPECT_EQ(20, tokenizer.lineBeginOffset());
        EXPECT_EQ(2, tokenizer.column());

        token = tokenizer.readNextToken({ rule });
        EXPECT_EQ(rule, token.spec);
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

        token = tokenizer.readNextToken({ whiteSpace, });
        EXPECT_EQ(nullptr, token.spec);
        token = tokenizer.readNextToken({ comment1 });
        EXPECT_EQ(comment1, token.spec);
        token = tokenizer.readNextToken({ whiteSpace });
        EXPECT_EQ(whiteSpace, token.spec);
        EXPECT_EQ(34, tokenizer.tokenStartOffset());
        EXPECT_EQ(36, tokenizer.tokenEndOffset());
        EXPECT_EQ(1, tokenizer.tokenStartLine());
        EXPECT_EQ(2, tokenizer.tokenEndLine());
        EXPECT_EQ(14, tokenizer.tokenStartColumn());
        EXPECT_EQ(1, tokenizer.tokenEndColumn());
        EXPECT_EQ(36, tokenizer.cursor());
        EXPECT_EQ(35, tokenizer.lineBeginOffset());
        EXPECT_EQ(1, tokenizer.column());

        token = tokenizer.readNextToken({ whiteSpace, comment1, });
        EXPECT_EQ(tokenizer.eosTokenSpec(), token.spec);
    }

    TEST(BuildFileTokenizer, commentLines) {
        const std::string commentLines(R"(         
        /* c1
         * c2
        c3
        c4 
  */  :)");
        BuildFileTokenizer tokenizer("testFile", commentLines);
        Token token;
        tokenizer.skip({ comment1, commentN, whiteSpace });
        token = tokenizer.readNextToken({ rule});
        EXPECT_EQ(rule, token.spec);
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
        token = tokenizer.readNextToken({ });
        EXPECT_EQ(tokenizer.eosTokenSpec(), token.spec);
    }

    TEST(BuildFileTokenizer, not) {
        const std::string notGlob(R"(^aap.c)");
        BuildFileTokenizer tokenizer("testFile", notGlob);
        Token token;
        token = tokenizer.readNextToken({ignore, glob});
        EXPECT_EQ(ignore, token.spec);
        EXPECT_EQ("^", token.value);
        token = tokenizer.readNextToken({glob});
        EXPECT_EQ(glob, token.spec);
        EXPECT_EQ("path", token.type);
        EXPECT_EQ("aap.c", token.value);
        EXPECT_TRUE(tokenizer.eos());
        token = tokenizer.readNextToken({ });
        EXPECT_EQ(tokenizer.eosTokenSpec(), token.spec);
    }

    TEST(BuildFileTokenizer, relativePath3) {
        const std::string path(R"(aap\noot\mies.txt)");
        BuildFileTokenizer tokenizer("testFile", path);
        Token token;
        token = tokenizer.readNextToken({glob});
        EXPECT_EQ(glob, token.spec);
        EXPECT_EQ("path", token.type);
        EXPECT_EQ(path, token.value);
        token = tokenizer.readNextToken({});
        EXPECT_EQ(tokenizer.eosTokenSpec(), token.spec);
    }

    TEST(BuildFileTokenizer, relativePath1) {
        const std::string path(R"(mies.txt)");
        BuildFileTokenizer tokenizer("testFile", path);
        Token token;
        token = tokenizer.readNextToken({glob});
        EXPECT_EQ(glob, token.spec);
        EXPECT_EQ("path", token.type);
        EXPECT_EQ(path, token.value);
        token = tokenizer.readNextToken({ });
        EXPECT_EQ(tokenizer.eosTokenSpec(), token.spec);
    }

    TEST(BuildFileTokenizer, absolutePath3) {
        const std::string path(R"(\aap\noot\mies.txt)");
        BuildFileTokenizer tokenizer("testFile", path);
        Token token;
        token = tokenizer.readNextToken({glob});
        EXPECT_EQ(glob, token.spec);
        EXPECT_EQ("path", token.type);
        EXPECT_EQ(path, token.value);
        token = tokenizer.readNextToken({});
        EXPECT_EQ(tokenizer.eosTokenSpec(), token.spec);
    }

    TEST(BuildFileTokenizer, absolutePath1) {
        const std::string path(R"(mies.txt)");
        BuildFileTokenizer tokenizer("testFile", path);
        Token token;
        token = tokenizer.readNextToken({glob});
        EXPECT_EQ(glob, token.spec);
        EXPECT_EQ("path", token.type);
        EXPECT_EQ(path, token.value);
        token = tokenizer.readNextToken({});
        EXPECT_EQ(tokenizer.eosTokenSpec(), token.spec);
    }

    TEST(BuildFileTokenizer, symbolicPath) {
        const std::string path(R"(@@repo\file)");
        BuildFileTokenizer tokenizer("testFile", path);
        Token token;
        token = tokenizer.readNextToken({ glob });
        EXPECT_EQ(glob, token.spec);
        EXPECT_EQ("path", token.type);
        EXPECT_EQ(path, token.value);
        token = tokenizer.readNextToken({glob});
        EXPECT_EQ(tokenizer.eosTokenSpec(), token.spec);
        EXPECT_EQ("", token.value);
    }

    TEST(
        BuildFileTokenizer, inputGroup) {
        const std::string path(R"(..\submodules\<object>)");
        BuildFileTokenizer tokenizer("testFile", path);
        Token token;
        token = tokenizer.readNextToken({glob});
        EXPECT_EQ(glob, token.spec);
        EXPECT_EQ("group", token.type);
        EXPECT_EQ(R"(..\submodules\<object>)", token.value);
        tokenizer.skip({ whiteSpace });
        token = tokenizer.readNextToken({ });
        EXPECT_EQ(tokenizer.eosTokenSpec(), token.spec);
    }

    TEST(BuildFileTokenizer, relativeGlob1) {
        const std::string globStr(R"(aap\a?b?[cde]*.txt)");
        BuildFileTokenizer tokenizer("testFile", globStr);
        Token token;
        token = tokenizer.readNextToken({glob});
        EXPECT_EQ(glob, token.spec);
        EXPECT_EQ("glob", token.type);
        EXPECT_EQ(globStr, token.value);
        token = tokenizer.readNextToken({});
        EXPECT_EQ(tokenizer.eosTokenSpec(), token.spec);
    }

    TEST(BuildFileTokenizer, absoluteGlob1) {
        const std::string globStr(R"(\aap\a?b?[cde]*.txt)");
        BuildFileTokenizer tokenizer("testFile", globStr);
        Token token;
        token = tokenizer.readNextToken({glob});
        EXPECT_EQ(glob, token.spec);
        EXPECT_EQ("glob", token.type);
        EXPECT_EQ(globStr, token.value);
        token = tokenizer.readNextToken({ });
        EXPECT_EQ(tokenizer.eosTokenSpec(), token.spec);
    }

    TEST(BuildFileTokenizer, singleLineScript) {
        const std::string cmdStr(R"(gcc src\hello.c -o bin\hello)");
        const std::string scriptStr = R"(|>gcc src\hello.c -o bin\hello|>)";

        BuildFileTokenizer tokenizer("testFile", scriptStr);
        Token token;

        token = tokenizer.readNextToken({script});
        EXPECT_EQ(script, token.spec);
        EXPECT_EQ(cmdStr, token.value);
        token = tokenizer.readNextToken({});
        EXPECT_EQ(tokenizer.eosTokenSpec(), token.spec);
    }

    TEST(BuildFileTokenizer, multiLineScript) {
        const std::string groupStr(R"(
                gcc 
                src\hello.c 
               -o bin\hello)");
        const std::string scriptStr = R"(|>
                gcc 
                src\hello.c 
               -o bin\hello|> )";

        BuildFileTokenizer tokenizer("testFile", scriptStr);
        Token token;

        token = tokenizer.readNextToken({script});
        EXPECT_EQ(script, token.spec);
        EXPECT_EQ(groupStr, token.value);
        tokenizer.skip({ whiteSpace });
        token = tokenizer.readNextToken({ whiteSpace });
        EXPECT_EQ(tokenizer.eosTokenSpec(), token.spec);
    }
    TEST(BuildFileTokenizer, rule) {
        const std::string commandStr(R"(
                gcc 
                src\hello.c 
               -o bin\hello 
            )");
        const std::string ruleStr(R"(: 
            src\hello.c |>
                gcc 
                src\hello.c 
               -o bin\hello 
            |> bin\%B.obj )");

        BuildFileTokenizer tokenizer("testFile", ruleStr);
        Token token;

        tokenizer.skip({ whiteSpace });
        token = tokenizer.readNextToken({ rule, foreach });
        EXPECT_EQ(rule, token.spec);
        EXPECT_EQ(":", token.value);
        tokenizer.skip({ whiteSpace });
        token = tokenizer.readNextToken({ glob });
        EXPECT_EQ(glob, token.spec);
        EXPECT_EQ("path", token.type);
        EXPECT_EQ("src\\hello.c", token.value);
        tokenizer.skip({ whiteSpace });
        token = tokenizer.readNextToken({ script });
        EXPECT_EQ(script, token.spec);
        EXPECT_EQ(commandStr, token.value);
        tokenizer.skip({ whiteSpace });
        token = tokenizer.readNextToken({ glob });
        EXPECT_EQ(glob, token.spec);
        EXPECT_EQ("path", token.type);
        EXPECT_EQ("bin\\%B.obj", token.value);
        tokenizer.skip({ whiteSpace });
        token = tokenizer.readNextToken({ whiteSpace });
        EXPECT_EQ(tokenizer.eosTokenSpec(), token.spec);
    }

    TEST(BuildFileTokenizer, foreachRule) {
        const std::string commandStr(R"(
                gcc 
                src\hello.c 
               -o bin\hello 
            )");
        const std::string ruleStr(R"(: 
            foreach src\hello.c |>
                gcc 
                src\hello.c 
               -o bin\hello 
            |> bin\%B.obj )");

        BuildFileTokenizer tokenizer("testFile", ruleStr);
        Token token;

        tokenizer.skip({ whiteSpace });
        token = tokenizer.readNextToken({rule});
        EXPECT_EQ(rule, token.spec);
        EXPECT_EQ(":", token.value);
        tokenizer.skip({ whiteSpace });
        token = tokenizer.readNextToken({foreach, glob});
        EXPECT_EQ(foreach, token.spec);
        tokenizer.skip({ whiteSpace });
        token = tokenizer.readNextToken({ glob });
        EXPECT_EQ(glob, token.spec);
        EXPECT_EQ("path", token.type);
        EXPECT_EQ("src\\hello.c", token.value);
        tokenizer.skip({ whiteSpace });
        token = tokenizer.readNextToken({ script });
        EXPECT_EQ(script, token.spec);
        EXPECT_EQ(commandStr, token.value);
        tokenizer.skip({ whiteSpace });
        token = tokenizer.readNextToken({ glob });
        EXPECT_EQ(glob, token.spec);
        EXPECT_EQ("path", token.type);
        EXPECT_EQ("bin\\%B.obj", token.value);
        tokenizer.skip({ whiteSpace });
        token = tokenizer.readNextToken({ whiteSpace });
        EXPECT_EQ(tokenizer.eosTokenSpec(), token.spec);
    }

    TEST(BuildFileTokenizer, command) {
        const std::string commandStr(R"(
                gcc 
                src\hello.c 
               -o bin\hello 
            )");
        const std::string ruleStr(R"(: 
            |>
                gcc 
                src\hello.c 
               -o bin\hello 
            |> bin\%B.obj )");
        BuildFileTokenizer tokenizer("testFile", ruleStr);
        Token token;

        tokenizer.skip({ whiteSpace });
        token = tokenizer.readNextToken({ rule });
        EXPECT_EQ(rule, token.spec);
        EXPECT_EQ(":", token.value);
        tokenizer.skip({ whiteSpace });
        token = tokenizer.readNextToken({ whiteSpace, cmdStart });
        EXPECT_EQ(cmdStart, token.spec);
        EXPECT_EQ("cmdStart", token.type);
        EXPECT_EQ("|>", token.value);
        tokenizer.skip({ whiteSpace });
        token = tokenizer.readNextToken({ cmdEnd });
        EXPECT_EQ(cmdEnd, token.spec);
        EXPECT_EQ("cmdEnd", token.type);
        EXPECT_EQ("|>", token.value);

    }
}