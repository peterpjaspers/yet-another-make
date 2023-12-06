#pragma once

#include <vector>
#include <memory>
#include <filesystem>
#include <regex>

namespace YAM {

    namespace BuildFile {
        struct __declspec(dllexport) Node {
            virtual ~Node() {}
            Node() : line(0), column(0) {}
            std::filesystem::path buildFile;
            std::size_t line;
            std::size_t column;
        };
        
        struct __declspec(dllexport) Input : public Node {
            bool exclude;
            std::filesystem::path pathPattern;
        };
        struct __declspec(dllexport) Inputs : public Node {
            std::vector<Input> inputs;
        };

        struct __declspec(dllexport) Script: public Node {
            std::string script;
        };

        struct __declspec(dllexport) Output : public Node {
            bool ignore;
            std::string path;
        };
        struct __declspec(dllexport) Outputs : public Node {
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
