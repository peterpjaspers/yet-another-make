#include "BuildState.h"
#include "Node.h"
#include "FileRepository.h"

namespace
{
    using namespace YAM;

    Delegate<bool, std::shared_ptr<Node> const&> includeIfDirty =
        Delegate<bool, std::shared_ptr<Node> const&>::CreateLambda(
            [](std::shared_ptr<Node> const& node)
    {
        return node->state() == Node::State::Dirty;
    });

    static std::shared_ptr<FileRepository> nullRepo;
}

namespace YAM
{
    BuildState::BuildState() {
        auto const& entireFileSet = FileAspectSet::entireFileSet();
        _fileAspectSets.insert({ entireFileSet.name(), entireFileSet });
    }

    bool BuildState::addRepository(std::shared_ptr<FileRepository> repo)
    {
        std::shared_ptr<FileRepository> duplicateRepo = findRepository(repo->name());
        bool duplicateName = nullptr != duplicateRepo;
        if (!duplicateName) {
            _repositories.insert({ repo->name(), repo });
        }
        return !duplicateName;
    }

    bool BuildState::removeRepository(std::string const& repoName) {
        auto it = _repositories.find(repoName);
        bool found = it != _repositories.end();
        if (found) {
            auto srcRepo = it->second;
            srcRepo->clear();
            _repositories.erase(it);
        }
        return found;
    }

    std::shared_ptr<FileRepository> const& BuildState::findRepository(std::string const& repoName) const {
        auto const it = _repositories.find(repoName);
        bool found = it != _repositories.end();
        if (found) {
            auto const& repo = it->second;
            return repo;
        }
        return nullRepo;
    }

    std::shared_ptr<FileRepository> const& BuildState::findRepositoryContaining(std::filesystem::path const& path) const {
        for (auto const& pair : _repositories) {
            auto const& repo = pair.second;
            if (repo->lexicallyContains(path)) return repo;
        }
        return nullRepo;
    }

    std::map<std::string, std::shared_ptr<FileRepository>> const& BuildState::repositories() const {
        return _repositories;
    }

    // Return the file aspects applicable to the file with the given path name.
    // Order the aspects in the returned vector by ascending aspect name.
    std::vector<FileAspect> BuildState::findFileAspects(std::filesystem::path const& path) const {
        std::vector<FileAspect> applicableAspects;
        applicableAspects.push_back(FileAspect::entireFileAspect());
        return applicableAspects;
    }

    // Return the file aspect set identified by the given name.
    // Throw exception when no such set exists.
    FileAspectSet const& BuildState::findFileAspectSet(std::string const& aspectSetName) const {

        auto const& s = _fileAspectSets.find(aspectSetName);
        if (s == _fileAspectSets.cend()) throw std::runtime_error("no such FileAspectSet");
        return s->second;
    }

    NodeSet& BuildState::nodes() {
        return _nodes;
    }

    void BuildState::getDirtyNodes(std::vector<std::shared_ptr<Node>>& dirtyNodes) {
        _nodes.find(includeIfDirty, dirtyNodes);
    }

    void BuildState::getBuildState(std::unordered_set<std::shared_ptr<IPersistable>>& buildState) {
        auto addToState =
            Delegate<void, std::shared_ptr<Node> const&>::CreateLambda(
                [&](std::shared_ptr<Node> node) {buildState.insert(node); });

        _nodes.foreach(addToState);
        for (auto const& pair : _repositories) { buildState.insert(pair.second); }
    }

    void BuildState::clearBuildState() {
        _repositories.clear();
        _nodes.clear();
    }

    void BuildState::computeStorageNeed(
        std::unordered_set<std::shared_ptr<IPersistable>> const& buildState,
        std::unordered_set<std::shared_ptr<IPersistable>> const& storedState,
        std::unordered_set<std::shared_ptr<IPersistable>>& toInsert,
        std::unordered_set<std::shared_ptr<IPersistable>>& toReplace,
        std::unordered_set<std::shared_ptr<IPersistable>>& toRemove
    ) {
        for (auto const& p : buildState) {
            if (storedState.contains(p)) {
                if (p->modified()) toReplace.insert(p);
            } else {
                p->modified(true);
                toInsert.insert(p);
            }
        }
        for (auto const& p : storedState) {
            if (!buildState.contains(p)) {
                toRemove.insert(p);
                // p may have been modified before it was remooved
                if (p->modified()) toReplace.insert(p);
            }
        }
    }

}