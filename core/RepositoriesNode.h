#pragma once

#include "RegexSet.h"
#include "Node.h"
#include "AcyclicTrail.h"
#include "xxhash.h"

#include <memory>
#include <filesystem>
#include <map>
#include <list>

namespace YAM
{
    class DirectoryNode;
    class SourceFileNode;
    class ExecutionContext;
    class FileRepositoryNode;

    // A RepositoriesNode parses file yamConfig/repositories.txt in the root 
    // directory of the home repository. The home repository (the repo with
    // name ".") is the repo from which the build is started.
    // This file lists the input repositories of the home repository, i.e.
    // the repositories from which files are read when building the home repo.
    // The parse result is a graph of FileRepositoryNode instances, accessible via
    // ExecutionContext::homeRepository().
    //
    // Syntax of file yamConfig/repositories.txt:
    //     File :== { Repo }*
    //     Repo :== RepoName Dir Type [InputRepos] ";'
    //     RepoName :== "name" "=" identifier
    //     Dir :== "dir" "=" path (relative to home repo or absolute)
    //     Type :== "type" "=" Integrated" | "Coupled" | "Tracked" | "Ignored"
    //     InputRepos :== inputs "=" { RepoName }*
    //
    // See clas FileRepositoryNode for an explanation of Type.
    // 
    // Cycles in the repository graph are not allowed.
    // InputRepos declarations are mandatory for dependencies on Coupled repos
    // because they define the order in which Coupled repos are built.
    // 
    // Best practice when deleting/moving a child root directory is to
    // first update the configuration file, then run yam, then do the
    // deletion/move.
    // First deleting the child root directory, then run yam: yam will
    // raise a warning: root directory of child repo does not exist.
    // First deleting the child root directory, then update config file, then
    // run yam: no problem.
    // First moving the child root directory, then run yam: yam will raise
    // a warning: root directory of child repo does not exist.
    // First moving the child root directory, then update config file, then
    // run yam: yam will raise an error: stale output files in moved directory.
    // The user has to delete the stale output files and restart the build.
    // 
    class __declspec(dllexport) RepositoriesNode : 
        public Node, 
        std::enable_shared_from_this<RepositoriesNode>
    {
    public:
        struct Repo {
            std::string name;
            std::filesystem::path dir;
            std::string type;
            std::vector<std::string> inputs;
        };

        RepositoriesNode(); // needed for deserialization
        // name is the name of the home repository
        RepositoriesNode(
            ExecutionContext* context, 
            std::shared_ptr<FileRepositoryNode> const& homeRepo);

        // Return the path of the repositories configuration file relative 
        // to the repository root directory.
        static std::filesystem::path configFilePath() {
            return "yamConfig/repositories.txt";
        }

        void ignoreConfigFile(bool ignore);
        bool ignoreConfigFile() const;

        // Return the absolute path of the child repositories configuration file.
        std::filesystem::path absoluteConfigFilePath() const;

        std::shared_ptr<FileRepositoryNode> const& homeRepository() const;

        std::map<std::string, std::shared_ptr<FileRepositoryNode>> const& repositories() const;
        bool addRepository(std::shared_ptr<FileRepositoryNode> const& repo);
        bool removeRepository(std::string const& repoName);
        std::shared_ptr<FileRepositoryNode> const& findRepository(std::string const& repoName) const;
        std::shared_ptr<FileRepositoryNode> const& findRepositoryContaining(std::filesystem::path const& path) const;
            
        // Start/stop watching the directory trees of all watchable repositories.
        void startWatching();
        void stopWatching();

        // For synchronous update.
        bool parseAndUpdate();

        // Inherited from Node
        void start() override;
        std::string className() const override { return "RepositoriesNode"; }

        static void setStreamableType(uint32_t type);
        // Inherited from IStreamer (via IPersistable)
        uint32_t typeId() const override;
        void stream(IStreamer* streamer) override;
        // Inherited from IPersistable
        void prepareDeserialize() override;
        bool restore(void* context, std::unordered_set<IPersistable const*>& restored) override;

    private:

        void handleRequisitesCompletion(Node::State newState);
        bool updateRepos(std::map<std::string, RepositoriesNode::Repo> const& repos);
        bool updateRepoDirectory(FileRepositoryNode& frepo, std::filesystem::path const& newDir);

        bool _ignoreConfigFile;
        std::shared_ptr<SourceFileNode> _configFile;
        std::shared_ptr<FileRepositoryNode> _homeRepo;
        std::map<std::string, std::shared_ptr<FileRepositoryNode>> _repositories;
        XXH64_hash_t _configFileHash;
        bool _modified;
    };
}