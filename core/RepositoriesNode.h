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
    // directory of the home repository. The home repository is the repo from
    // which the build is started.
    // This file lists the repositories on which the home repository depends.
    //
    // Syntax of file yamConfig/repositories.txt:
    //     File :== { Repo }*
    //     Repo :== RepoName Dir Type ";'
    //     RepoName :== "name" "=" identifier
    //     Dir :== "dir" "=" path (relative to home repo or absolute)
    //     Type :== "type" "=" Build" | "Track" | "Ignore"
    //
    // See class FileRepositoryNode.
    // 
    // Deleting/renaming/moving a repository directory while yam is running 
    // will not be detected by yam because yam is not monitoring the parent
    // directory of the repository.
    // 
    // Best practice when deleting a repository directory is to first remove
    // the repository from the configuration file, then run yam, then delete 
    // the repository directory.
    //  
    // Best practice when renaming/moving a repository directory is to first
    // update the configuration file with the new directory name, then 
    // rename/move the directory, then run yam.
    // 
    // First renaming/moving the repo directory, then update config file, then
    // run yam: yam will raise an error: stale output files in moved directory.
    // The user has to delete the stale output files and restart the build.
    // 
    class __declspec(dllexport) RepositoriesNode : public Node
    {
    public:
        struct Repo {
            std::string name;
            std::filesystem::path dir;
            std::string type;
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

        XXH64_hash_t hash() const;

        // Inherited from Node
        void start(PriorityClass prio) override;
        std::string className() const override { return "RepositoriesNode"; }

        static void setStreamableType(uint32_t type);
        // Inherited from IStreamer (via IPersistable)
        uint32_t typeId() const override;
        void stream(IStreamer* streamer) override;
        // Inherited from IPersistable
        void prepareDeserialize() override;
        bool restore(void* context, std::unordered_set<IPersistable const*>& restored) override;

    private:
        XXH64_hash_t computeHash() const;

        void handleRequisitesCompletion(Node::State newState);
        bool updateRepos(std::map<std::string, RepositoriesNode::Repo> const& repos);
        bool updateRepoDirectory(FileRepositoryNode& frepo, std::filesystem::path const& newDir);

        bool _ignoreConfigFile;
        std::shared_ptr<SourceFileNode> _configFile;
        std::shared_ptr<FileRepositoryNode> _homeRepo;
        std::map<std::string, std::shared_ptr<FileRepositoryNode>> _repositories;
        XXH64_hash_t _configFileHash;
        XXH64_hash_t _hash; // hash of the repository hashes
        bool _modified;
    };
}