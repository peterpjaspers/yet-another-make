#pragma once

#include "BuildFileTokenizer.h"

namespace {
    using namespace YAM;

    std::vector<TokenSpec> tokenSpecs = {
        TokenSpec(R"(^\s+)", "skip"), // whitespace
        TokenSpec(R"(^\/\/.*)", "skip"), // single-line comment
        TokenSpec(R"(^\/\*[\s\S]*?\*\/)", "skip"), // multi-line comment
        TokenSpec(R"(^deps)", "deps"),
        TokenSpec(R"(^buildfile)", "depBuildfile"),
        TokenSpec(R"(^glob)", "depGlob"),
        TokenSpec(R"(^\{)", "{"),
        TokenSpec(R"(^\})", "}"),
        TokenSpec(R"(^foreach)", "foreach"),
        TokenSpec(R"(^:)", "rule"),
        TokenSpec(R"(^\^)", "not"),
        TokenSpec(R"(^\\?(?:[\w\.\*\?\%\[\]-])+(?:\\([\w\.\*\?\%\[\]-])+)*)", "glob"),
        TokenSpec(R"(^\|>(((?!\|>)\S|\s)*)\|>)", "script", 1),
    };
}
