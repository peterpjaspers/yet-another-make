#pragma once

#include "Delegates.h"
#include "ExecutionContext.h"
#include "Node.h"

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
        void logRepoNotInitialized();
        bool _init(std::shared_ptr<BuildRequest> const& request);
        void _start();
        void _clean(std::shared_ptr<BuildRequest> request);
        void _handleConfigNodesCompletion(Node* n);
        void _handleDirectoriesCompletion(Node* n);
        void _handleBuildFileParsersCompletion(Node* n);
        void _handleBuildFileCompilersCompletion(Node* n);
        bool _containsBuildFileCycles(std::vector<std::shared_ptr<Node>> const& buildFileParserNodes) const;
        bool _containsGroupCycles(std::vector<std::shared_ptr<Node>> const& buildFileCompilerNodes) const;
        void _handleCommandsCompletion(Node* n);
        void _postCompletion(Node::State resultState);
        void _notifyCompletion(Node::State resultState);
        void _storeBuildState(bool logSave = false);

        ExecutionContext _context;
        std::shared_ptr<PersistentBuildState> _buildState;
        MulticastDelegate<std::shared_ptr<BuildResult>> _completor;

        std::shared_ptr<GroupNode> _dirtyConfigNodes;
        std::shared_ptr<GroupNode> _dirtyDirectories;
        std::shared_ptr<GroupNode> _dirtyBuildFileParsers;
        std::shared_ptr<GroupNode> _dirtyBuildFileCompilers;
        std::shared_ptr<GroupNode> _dirtyCommands;
        std::shared_ptr<BuildResult> _result;
    };
}


