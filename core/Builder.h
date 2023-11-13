#pragma once

#include "Delegates.h"
#include "ExecutionContext.h"

#include <memory>
#include <filesystem>

namespace YAM
{
    class BuildRequest;
    class BuildResult;
    class CommandNode;

    class __declspec(dllexport) Builder
    {
    public:
        Builder(bool enableRepoMirroring = true);
        virtual ~Builder();

        void enableRepoMirroring(bool enable)  {
            _repoMirroringEnabled = enable;
        }

        bool repoMirroringEnabled() const {
            return _repoMirroringEnabled;
        }

        ExecutionContext* context();

        // Execute given 'request' asynchronously.
        // Notify completion by calling completor()->Broadcast(result).
        // Take care: completion will be notified in different thread then
        // the one that called start().
        void start(std::shared_ptr<BuildRequest> request);

        // Return whether a build is running.
        bool running();

        // Stop a running build.
        // Completion will be notified as described in start().
        // Stopping a build that is not running or that has already
        // completed is a no-op.
        void stop();

        // Return delegate to which clients can add callbacks that will be
        // executed when build execution completes. See start(). 
        MulticastDelegate<std::shared_ptr<BuildResult>>& completor();

    private:
        void _init(std::filesystem::path directory);
        void _start();
        void _clean(std::shared_ptr<BuildRequest> request);
        void _handleSourcesCompletion(Node* n);
        void _handleBuildFilesCompletion(Node* n);
        void _handleCommandsCompletion(Node* n);
        void _notifyCompletion();

        bool _repoMirroringEnabled;
        ExecutionContext _context;
        MulticastDelegate<std::shared_ptr<BuildResult>> _completor;

        // Dirty source directory and source file nodes, dirty build file 
        // nodes and dirty command nodes are collected as the input producers
        // of command nodes. This avoids duplicating execution logic already 
        // present in CommandNode (and its baseclass Node).
        std::shared_ptr<CommandNode> _dirtySources;
        std::shared_ptr<CommandNode> _dirtyBuildFiles;
        std::shared_ptr<CommandNode> _dirtyCommands;
        std::shared_ptr<BuildResult> _result;
    };
}


