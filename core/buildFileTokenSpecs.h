#pragma once

#include "BuildFileTokenizer.h"

namespace {
    using namespace YAM;

    std::vector<TokenSpec> tokenSpecs = {
        TokenSpec(R"(^\s+)", "skip"), // whitespace
        TokenSpec(R"(^\/\/.*)", "skip"), // single-line comment
        TokenSpec(R"(^\/\*[\s\S]*?\*\/)", "skip"), // multi-line comment
        TokenSpec(R"(^buildfile)", "depBuildFile"),
        TokenSpec(R"(^glob)", "depGlob"),
        TokenSpec(R"(^:)", "rule"),
        TokenSpec(R"(^foreach)", "foreach"),
        TokenSpec(R"(^\^)", "not"),
        TokenSpec(R"(^\{)", "{"),
        TokenSpec(R"(^\})", "}"),
        TokenSpec(R"(^\|>(((?!\|>)\S|\s)*)\|>)", "script", 1),
        TokenSpec(R"(^\|[^>])", "|"),
        TokenSpec(R"(^[^\{\}\|]\\?(?:[\w\.\*\?\%\[\]-])+(?:\\([\w\.\*\?\%\[\]-])+)*)", "glob"),
    };
}
