#pragma once

#include "Delegates.h"
#include "ExecutionContext.h"

#include <memory>
#include <filesystem>

namespace YAM
{
    class BuildRequest;
    class BuildResult;
    class GroupNode;
    class PersistentBuildState;

    class __declspec(dllexport) Builder
    {
    public:
        Builder();
        virtual ~Builder();

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
        void _handleDirectoriesCompletion(Node* n);
        void _handleBuildFileParsersCompletion(Node* n);
        void _handleBuildFileCompilersCompletion(Node* n);
        bool containsCycles(std::vector<std::shared_ptr<Node>> const& buildFileParserNodes) const;
        void _handleCommandsCompletion(Node* n);
        void _notifyCompletion();

        ExecutionContext _context;
        std::shared_ptr<PersistentBuildState> _buildState;
        MulticastDelegate<std::shared_ptr<BuildResult>> _completor;

        std::shared_ptr<GroupNode> _dirtyDirectories;
        std::shared_ptr<GroupNode> _dirtyBuildFileParsers;
        std::shared_ptr<GroupNode> _dirtyBuildFileCompilers;
        std::shared_ptr<GroupNode> _dirtyCommands;
        std::shared_ptr<BuildResult> _result;
    };
}


