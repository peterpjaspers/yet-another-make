#pragma once

#include "Glob.h"

#include <vector>
#include <memory>
#include <filesystem>
#include <regex>

namespace YAM {
    namespace SyntaxTree {

        class __declspec(dllexport) Node {
        public:
            virtual ~Node() {}

            void add(std::shared_ptr<Node> const& child) {
                _children.push_back(child);
            }
            std::vector<std::shared_ptr<Node>> const& children() {
                return _children;
            }

        private:
            std::vector<std::shared_ptr<Node>> _children;
        };

        class __declspec(dllexport) BuildFile : public Node {};

        class __declspec(dllexport) Rule : public Node {
        public:
            Rule() : Node(), forEach(false) {}
            bool forEach;
        };

        class __declspec(dllexport) Inputs : public Node {};

        class __declspec(dllexport) Input : public Node {
        public:
            Input(bool excl, std::string const& globPattern) 
                : Node()
                , exclude(excl)
                , glob(globPattern, true)
            {}
            bool exclude; // from previous includes
            Glob glob;
        };

        class __declspec(dllexport) Script : public Node {
        public:
            std::string script;
        };

        class __declspec(dllexport) Outputs : public Node {};

        class __declspec(dllexport) Output : public Node {
        public:
            std::string path;
        };
    }
}
