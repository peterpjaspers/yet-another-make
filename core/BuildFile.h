#pragma once

#include "IStreamer.h"
#include "IStreamable.h"
#include "xxhash.h"

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

        virtual void addHashes(std::vector<XXH64_hash_t>& hashes) const;

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

        void addHashes(std::vector<XXH64_hash_t>& hashes) const override;
        void stream(IStreamer* streamer) override;
    };

    struct __declspec(dllexport) Inputs : public Node {
        std::vector<Input> inputs;

        void addHashes(std::vector<XXH64_hash_t>& hashes) const override;
        void stream(IStreamer* streamer) override;
    };

    struct __declspec(dllexport) Script: public Node {
        std::string script;

        void addHashes(std::vector<XXH64_hash_t>& hashes) const override;
        void stream(IStreamer* streamer) override ;
    };

    struct __declspec(dllexport) Output : public Node {
        bool ignore;
        std::filesystem::path path;
        PathType pathType;

        void addHashes(std::vector<XXH64_hash_t>& hashes) const override;
        void stream(IStreamer* streamer) override;
        bool operator==(Output const& rhs) const;
    };

    struct __declspec(dllexport) Outputs : public Node {
        std::vector<Output> outputs;

        void addHashes(std::vector<XXH64_hash_t>& hashes) const override;
        void stream(IStreamer* streamer) override;
        bool operator==(Outputs const& rhs) const;
    };


    struct __declspec(dllexport) Rule : public Node {
        bool forEach;
        Inputs cmdInputs;
        Inputs orderOnlyInputs;
        Script script;
        Outputs outputs;
        std::vector<std::filesystem::path> outputGroups;
        std::vector<std::filesystem::path> bins;

        void addHashes(std::vector<XXH64_hash_t>& hashes) const override;
        uint32_t typeId() const override;
        void stream(IStreamer* streamer) override;
    }; 
        
    struct __declspec(dllexport) Deps : public Node
    {
        // Rules may use input files that are output files of rules defined 
        // in other buildfiles. Yam requires the user to declare dependencies
        // on these buildfiles to ensure that all output files are defined
        // before they are being referenced in rule input sections.
        std::vector<std::filesystem::path> depBuildFiles;

        // Buildfile content can be defined indirectly by buildfile scripts,
        // e.g. a Python script file. Yam executes these script files. The 
        // The script outputs buildfile text in Yam rule syntax. Yam 
        // detects and registers dependencies on files read during script 
        // execution and re-exeuctes the script when one or more of these 
        // dependencies change. 
        // The script may also read directories, typically by using globs,
        // e.g. to create a compilation rule for each source file. In this case
        // the script must re-execute when the glob result changes. Yam 
        // however cannot detect dependencies on directories/globs. The script
        // must therefore declare these glob dependencies in the output 
        // buildfile text. Yam will re-execute the script when glob results 
        // change.
        std::vector<std::filesystem::path> depGlobs;

        void addHashes(std::vector<XXH64_hash_t>& hashes) const override;
        void stream(IStreamer* streamer) override;
    };

    class __declspec(dllexport) File : public Node {
    public:
        std::filesystem::path buildFile;
        Deps deps;
        std::vector<std::shared_ptr<Node>> variablesAndRules;

        XXH64_hash_t computeHash() const;
        void stream(IStreamer* streamer) override;
    };
}}
