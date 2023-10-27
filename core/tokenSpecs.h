#pragma once

#include "Tokenizer.h"

namespace {
    using namespace YAM;

    std::vector<TokenSpec> tokenSpecs = {
        TokenSpec(R"(^\s+)", "skip"), // whitespace
        TokenSpec(R"(^\/\/.*)", "skip"), // single-line comment
        TokenSpec(R"(^\/\*[\s\S]*?\*\/)", "skip"), // multi-line comment
        TokenSpec(R"(^:)", "rule"),
        TokenSpec(R"(^\^)", "not"),
        TokenSpec(R"(^\\?(?:[\w\.\*\?\[\]-])+(?:\\([\w\.\*\?\[\]-])+)*)", "glob"),
        TokenSpec(R"(^\|>(((?!\|>)\S|\s)*)\|>)", "script", 1),
    };
}
