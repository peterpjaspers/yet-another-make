#pragma once

#include "../xxHash/xxhash.h"

#include <vector>
#include <memory>
#include <filesystem>
#include <regex>

namespace YAM {

    namespace BuildFile {
        struct __declspec(dllexport) Node {
            virtual ~Node() {}
            Node() : line(0), column(0) {}
            std::size_t line;
            std::size_t column;

            virtual void addHashes(std::vector<XXH64_hash_t>& hashes) {
                hashes.push_back(line);
                hashes.push_back(column);
            }
        };
        
        struct __declspec(dllexport) Input : public Node {
            bool exclude;
            std::filesystem::path pathPattern;

            void addHashes(std::vector<XXH64_hash_t>& hashes) override {
                Node::addHashes(hashes);
                hashes.push_back(exclude);
                hashes.push_back(XXH64_string(pathPattern.string()));
            }
        };
        struct __declspec(dllexport) Inputs : public Node {
            std::vector<Input> inputs;

            void addHashes(std::vector<XXH64_hash_t>& hashes) override {
                Node::addHashes(hashes);
                for (auto &input : inputs) {
                    input.addHashes(hashes);
                }
            }
        };

        struct __declspec(dllexport) Script: public Node {
            std::string script;

            void addHashes(std::vector<XXH64_hash_t>& hashes) override {
                Node::addHashes(hashes);
                hashes.push_back(XXH64_string(script));
            }
        };

        struct __declspec(dllexport) Output : public Node {
            bool ignore;
            std::filesystem::path path;

            void addHashes(std::vector<XXH64_hash_t>& hashes) override {
                Node::addHashes(hashes);
                hashes.push_back(ignore);
                hashes.push_back(XXH64_string(path.string()));
            }
        };
        struct __declspec(dllexport) Outputs : public Node {
            std::vector<Output> outputs;

            void addHashes(std::vector<XXH64_hash_t>& hashes) override {
                Node::addHashes(hashes);
                for (auto &output : outputs) {
                    output.addHashes(hashes);
                }
            }
        };

        struct __declspec(dllexport) Rule : public Node {
            bool forEach;
            Inputs cmdInputs;
            Inputs orderOnlyInputs;
            Script script;
            Outputs outputs;

            void addHashes(std::vector<XXH64_hash_t>& hashes) override {
                Node::addHashes(hashes);
                hashes.push_back(forEach);
                cmdInputs.addHashes(hashes);
                orderOnlyInputs.addHashes(hashes);
                script.addHashes(hashes);
                outputs.addHashes(hashes);
            }
        }; 
        
        struct __declspec(dllexport) Deps : public Node
        {
            std::vector<std::filesystem::path> depBuildFiles;
            std::vector<std::filesystem::path> depGlobs;

            void addHashes(std::vector<XXH64_hash_t>& hashes) override {
                Node::addHashes(hashes);
                for (auto const& path : depBuildFiles) {
                    hashes.push_back(XXH64_string(path.string()));
                }
                for (auto const& path : depGlobs) {
                    hashes.push_back(XXH64_string(path.string()));
                }
            }
        };

        class __declspec(dllexport) File : public Node {
        public:
            std::filesystem::path buildFile;
            Deps deps;
            std::vector<std::shared_ptr<Node>> variablesAndRules;

            XXH64_hash_t computeHash() {                
                std::vector<XXH64_hash_t> hashes;
                hashes.push_back(XXH64_string(buildFile.string()));
                Node::addHashes(hashes);
                deps.addHashes(hashes);
                for (auto & varOrRule : variablesAndRules) {
                    varOrRule->addHashes(hashes);
                }
                return XXH64(hashes.data(), sizeof(XXH64_hash_t) * hashes.size(), 0);
            }
        };
    }
}
