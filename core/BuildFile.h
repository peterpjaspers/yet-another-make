#pragma once

#include "IStreamer.h"
#include "IStreamable.h"
#include "../xxHash/xxhash.h"

#include <vector>
#include <memory>
#include <filesystem>
#include <regex>

namespace YAM { namespace BuildFile {

    enum PathType { Path=1, Glob=2, Group=3, Bin=4 };

    struct __declspec(dllexport) Node : public IStreamable {
        std::size_t line;
        std::size_t column;

        virtual ~Node();
        Node();

        virtual void addHashes(std::vector<XXH64_hash_t>& hashes);

        // Inherited from IStreamable
        uint32_t typeId() const override { 
            throw std::runtime_error("not supported"); 
        }
        void stream(IStreamer* streamer) override;
    };
        
    struct __declspec(dllexport) Input : public Node {
        bool exclude;
        std::filesystem::path path;
        PathType pathType;

        void addHashes(std::vector<XXH64_hash_t>& hashes) override;
        void stream(IStreamer* streamer) override;
    };

    struct __declspec(dllexport) Inputs : public Node {
        std::vector<Input> inputs;

        void addHashes(std::vector<XXH64_hash_t>& hashes) override;
        void stream(IStreamer* streamer) override;
    };

    struct __declspec(dllexport) Script: public Node {
        std::string script;

        void addHashes(std::vector<XXH64_hash_t>& hashes) override;
        void stream(IStreamer* streamer) override ;
    };

    struct __declspec(dllexport) Output : public Node {
        bool ignore;
        std::filesystem::path path;
        PathType pathType;

        void addHashes(std::vector<XXH64_hash_t>& hashes) override;
        void stream(IStreamer* streamer) override;
    };

    struct __declspec(dllexport) Outputs : public Node {
        std::vector<Output> outputs;

        void addHashes(std::vector<XXH64_hash_t>& hashes) override;
        void stream(IStreamer* streamer) override;
    };


    struct __declspec(dllexport) Rule : public Node {
        bool forEach;
        Inputs cmdInputs;
        Inputs orderOnlyInputs;
        Script script;
        Outputs outputs;
        std::vector<std::filesystem::path> outputGroups;
        std::vector<std::filesystem::path> bins;

        void addHashes(std::vector<XXH64_hash_t>& hashes) override;
        uint32_t typeId() const override;
        void stream(IStreamer* streamer) override;
    }; 
        
    struct __declspec(dllexport) Deps : public Node
    {
        std::vector<std::filesystem::path> depBuildFiles;
        std::vector<std::filesystem::path> depGlobs;

        void addHashes(std::vector<XXH64_hash_t>& hashes) override;
        void stream(IStreamer* streamer) override;
    };

    class __declspec(dllexport) File : public Node {
    public:
        std::filesystem::path buildFile;
        Deps deps;
        std::vector<std::shared_ptr<Node>> variablesAndRules;

        XXH64_hash_t computeHash();
        void stream(IStreamer* streamer) override;
    };
}}
