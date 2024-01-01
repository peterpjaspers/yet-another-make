#pragma once

#include "NodeSet.h"
#include "FileAspect.h"
#include "FileAspectSet.h"

#include <map>
#include <vector>
#include <unordered_set>
#include <string>
#include <memory>
#include <filesystem>

// TODO:
//  - FileRepositoryWatcher not a member of FileRepository.
//  - FileRepositoryWatchers in ExecutionContext
//  - FileAspect(Set) as Node
//  - computeStorageNeeded to PersistenBuildState
//
namespace YAM
{
    class FileRepository;
    class Node;
    class IPersistable;

    class BuildState
    {
    public:
        BuildState();
        BuildState(BuildState const&) = delete;
        BuildState(BuildState &&) = delete;
        BuildState& operator = (const BuildState&&) = delete;

        // Add repository, return whether it was added, i..e had a unique name.
        bool addRepository(std::shared_ptr<FileRepository> repo);
        // Remove repository, return whether it was removed.
        bool removeRepository(std::string const& repoName);

        // Find repository by name, return found repo, nullptr when not found.
        std::shared_ptr<FileRepository> const& findRepository(std::string const& repoName) const;

        // Find repository that contains path, return found repo, nullptr when
        // not found.
        std::shared_ptr<FileRepository> const& findRepositoryContaining(std::filesystem::path const& path) const;

        // Return repositories.
        std::map<std::string, std::shared_ptr<FileRepository>> const& repositories() const;

        // Return the file aspects applicable to the file with the given path
        // name. A FileNode associated with the path will compute the hashes of
        // the applicable aspects.
        std::vector<FileAspect> findFileAspects(std::filesystem::path const& path) const;

        // Return the file aspect set identified by the given name.
        // A CommandNode uses this set to find for each input file the aspect 
        // that is relevant to the command. E.g. for a compile command the 
        // relevant aspect for *.h and *.cpp input files will be the code 
        // aspect. That aspect excludes comment section from the hash. The 
        // command uses the hash of the relevant aspect to compute the command 
        // execution hash. Goal: avoid re-execution of the command when only
        // non-relevant aspects of the file changes.
        // 
        FileAspectSet const& findFileAspectSet(std::string const& aspectSetName) const;

        NodeSet& nodes();

        // Return the nodes that are in state Node::State::Dirty
        void getDirtyNodes(std::vector<std::shared_ptr<Node>>& dirtyNodes);


        // Fill 'buildState' with nodes and repositories.
        void getBuildState(std::unordered_set<std::shared_ptr<IPersistable>>& buildState);

        // Post: nodes.empty() and repositories().empty()
        void clearBuildState();

        // Determine the differences between buildState and storedState. 
        // Post:
        //   toInsert: objects in buildState but not in storedState .
        //   toReplace: objects in buildState and storedState.
        //   toRemove: objects in storedState but not in buildState.
        //   objects in toInsert and toReplace are modified().
        void computeStorageNeed(
            std::unordered_set<std::shared_ptr<IPersistable>> const& buildState,
            std::unordered_set<std::shared_ptr<IPersistable>> const& storedState,
            std::unordered_set<std::shared_ptr<IPersistable>>& toInsert,
            std::unordered_set<std::shared_ptr<IPersistable>>& toReplace,
            std::unordered_set<std::shared_ptr<IPersistable>>& toRemove);

    private:
        std::map<std::string, std::shared_ptr<FileRepository>> _repositories;
        NodeSet _nodes;
        std::map<std::string, FileAspect> _fileAspects;
        std::map<std::string, FileAspectSet> _fileAspectSets;
    };
}
