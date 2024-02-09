#pragma once

#include "Node.h"
#include "../xxHash/xxhash.h"

namespace YAM
{
    class ExecutionContext;
    class SourceFileNode;
    class IStreamer;

    // This class provides, given the name of an executable file, the
    // command needed to execute that file. The command is used by to spawn
    // (invoke) a process that executes the file. This functionality is
    // used to execute buildfiles.
    // 
    // Available commands are configured in file yamConfig/fileExecSpecs.txt,
    // relative to the repository in which this node is created.
    // E.g. to execute a Python file someFile.py one needs to run the
    // Python interpreter as follows: python.exe someFile.py 
    // 
    // File syntax:
    //    File :== { Command }*
    //    Command :== Ext "=>" Fmt
    //    Ext :== file extension
    //    Fmt :== string running to the end of the line, containing 1 or
    //             more %f. %f will be replaced by the file name.
    //
    // Example:
    //     .bat => C:\Windows\System32\cmd.exe /c %f
    //     .cmd => C:\Windows\System32\cmd.exe /c %f
    //     .py => C:\Windows\py.exe %f
    //     .exe => %f
    //
    // Note that path names must be absolute because YAM runs spawned processes
    // with an empty environment.
    // 
    // Lines starting with // are comment lines.
    //
    class __declspec(dllexport) FileExecSpecsNode : public Node
    {
    public:
        FileExecSpecsNode(ExecutionContext* context, std::filesystem::path const& repoName);
        FileExecSpecsNode(); // Needed for deserialization.

        // Return the name of the configuration file relative to the
        // repository root directory.
        static std::filesystem::path configFilePath() {
            return "yamConfig/fileExecSpecs.txt";
        }

        // Return the absolute path of the configuration file.
        std::filesystem::path absoluteConfigFilePath() const;

        std::shared_ptr<SourceFileNode> configFileNode() const;

        // Return the command for given file name.
        // Return empty string when no command matches the file name
        // extension.
        std::string command(std::filesystem::path const& fileName) const;

        //Inherited from Node
        void start() override;
        std::string className() const override { return "FileExecSpecsNode"; }

        static void setStreamableType(uint32_t type);
        // Inherited from IStreamable
        uint32_t typeId() const override;
        void stream(IStreamer* streamer) override;
        // Inherited from IPersistable
        void prepareDeserialize() override;
        bool restore(void* context, std::unordered_set<IPersistable const*>& restored) override;

    private:
        void handleRequisitesCompletion(Node::State newState);
        bool parse();
        XXH64_hash_t computeExecutionHash() const;

        std::shared_ptr<SourceFileNode> _configFile;
        // maps stem to command format
        std::map<std::filesystem::path, std::string> _commandFmts;

        // The hash of content of _config;
        XXH64_hash_t _executionHash;
    };
}

