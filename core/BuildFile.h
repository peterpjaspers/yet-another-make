#pragma once

#include <vector>
#include <memory>
#include <filesystem>
#include <regex>

namespace YAM {

    namespace BuildFile {
        struct __declspec(dllexport) Node {
            virtual ~Node() {}
        };
        
        struct __declspec(dllexport) Input {
            bool exclude;
            std::filesystem::path pathPattern;
        };
        struct __declspec(dllexport) Inputs {
            std::vector<Input> inputs;
        };

        struct __declspec(dllexport) Script {
            std::string script;
        };

        struct __declspec(dllexport) Output {
            std::string path;
        };
        struct __declspec(dllexport) Outputs {
            std::vector<Output> outputs;
        };

        struct __declspec(dllexport) Rule : public Node {
            bool forEach;
            Inputs cmdInputs;
            Inputs orderOnlyInputs;
            Script script;
            Outputs outputs;
        };

        class __declspec(dllexport) File {
        public:
            std::vector<std::shared_ptr<Node>> variablesAndRules;
        };
    }
}
