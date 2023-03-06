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
        Builder();

        ExecutionContext* context();

        // Execute given 'request' asynchronously.
        // Notify completion by calling completor()->Broadcast(result).
        // Take care: completion will be notified in different thread then
        // the one that called start().
        void start(std::shared_ptr<BuildRequest> request);

        // Return whether a build is running.
        bool running() const;

        // Stop a running build.
        // Completion will be notified as described in start().
        void stop();

        // Return delegate to which clients can add callbacks that will be
        // executed when build execution completes. See start(). 
        MulticastDelegate<std::shared_ptr<BuildResult>>& completor();

    private:
        void _init(std::filesystem::path directory, bool failIfAlreadyInitialized);
        void _start();
        void _clean(std::shared_ptr<BuildRequest> request);
        void _stop();
        void _handleRepoCompletion(Node* n);
        void _handleBuildCompletion(Node* n);
        void _notifyCompletion();

        ExecutionContext _context;
        MulticastDelegate<std::shared_ptr<BuildResult>> _completor;

        // The nodes in build scope are collected in the input producers of
        // _scope. This avoids duplicating execution logic already present
        // in CommandNode (and its baseclass Node).
        // _scope == nullptr when no build is in progress.
        std::shared_ptr<CommandNode> _scope;
        std::shared_ptr<BuildResult> _result;
    };
}


