#pragma once

#include "IStreamer.h"
#include "IStreamable.h"
#include "../xxHash/xxhash.h"

#include <vector>
#include <memory>
#include <filesystem>
#include <regex>

namespace
{
    template<typename T>
    void streamVector(IStreamer* streamer, std::vector<T>& items) {
        uint32_t nItems;
        if (streamer->writing()) {
            const std::size_t length = items.size();
            if (length > UINT_MAX) throw std::exception("map too large");
            nItems = static_cast<uint32_t>(length);
        }
        streamer->stream(nItems);
        if (streamer->writing()) {
            for (std::size_t i = 0; i < nItems; ++i) {
                items[i].stream(streamer);
            }
        } else {
            T item;
            for (std::size_t i = 0; i < nItems; ++i) {
                item.stream(streamer);
                items.push_back(item);
            }
        }
    }
}
namespace YAM {
    namespace BuildFile {
        struct __declspec(dllexport) Node : public IStreamable {
            virtual ~Node() {}
            Node() : line(0), column(0) {}
            std::size_t line;
            std::size_t column;

            virtual void addHashes(std::vector<XXH64_hash_t>& hashes) {
                hashes.push_back(line);
                hashes.push_back(column);
            }

            uint32_t typeId() const override { throw std::runtime_error("not supported"); }
            // Inherited from IStreamable
            void stream(IStreamer* streamer) override {
                streamer->stream(line);
                streamer->stream(column);
            }
        };
        
        struct __declspec(dllexport) Input : public Node {
            bool exclude;
            std::filesystem::path pathPattern;
            bool isGroup;

            void addHashes(std::vector<XXH64_hash_t>& hashes) override {
                Node::addHashes(hashes);
                hashes.push_back(exclude);
                hashes.push_back(isGroup);
                hashes.push_back(XXH64_string(pathPattern.string()));
            }

            void stream(IStreamer* streamer) override {
                Node::stream(streamer);
                streamer->stream(exclude);
                streamer->stream(pathPattern);
                streamer->stream(isGroup);
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

            void stream(IStreamer* streamer) override {
                Node::stream(streamer);
                uint32_t length = static_cast<uint32_t>(inputs.size());
                streamVector(streamer, inputs);
            }
        };

        struct __declspec(dllexport) Script: public Node {
            std::string script;

            void addHashes(std::vector<XXH64_hash_t>& hashes) override {
                Node::addHashes(hashes);
                hashes.push_back(XXH64_string(script));
            }

            void stream(IStreamer* streamer) override {
                Node::stream(streamer);
                streamer->stream(script);
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

            void stream(IStreamer* streamer) override {
                Node::stream(streamer);
                streamer->stream(ignore);
                streamer->stream(path);
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

            void stream(IStreamer* streamer) override {
                Node::stream(streamer);
                streamVector(streamer, outputs);
            }
        };


        struct __declspec(dllexport) Rule : public Node {
            bool forEach;
            Inputs cmdInputs;
            Inputs orderOnlyInputs;
            Script script;
            Outputs outputs;
            std::filesystem::path outputGroup;

            void addHashes(std::vector<XXH64_hash_t>& hashes) override {
                Node::addHashes(hashes);
                hashes.push_back(forEach);
                cmdInputs.addHashes(hashes);
                orderOnlyInputs.addHashes(hashes);
                script.addHashes(hashes);
                outputs.addHashes(hashes);
                hashes.push_back(XXH64_string(outputGroup.string()));
            }

            uint32_t typeId() const override { throw std::runtime_error("not supported"); }
            void stream(IStreamer* streamer) override {
                Node::stream(streamer);
                cmdInputs.stream(streamer);
                orderOnlyInputs.stream(streamer);
                script.stream(streamer);
                outputs.stream(streamer);
                streamer->stream(outputGroup);
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

            void stream(IStreamer* streamer) override {
                Node::stream(streamer);
                streamer->streamVector(depBuildFiles);
                streamer->streamVector(depGlobs);
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

            void stream(IStreamer* streamer) override {
                Node::stream(streamer);
                streamer->stream(buildFile);
                deps.stream(streamer);
                streamer->streamVector(variablesAndRules);
            }
        };
    }
}
