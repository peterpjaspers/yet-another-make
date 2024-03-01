#include "RepositoriesNode.h"
#include "SourceFileNode.h"
#include "DirectoryNode.h"
#include "BuildFileParserNode.h"
#include "BuildFileCompilerNode.h"
#include "ExecutionContext.h"
#include "FileRepositoryNode.h"
#include "BuildFileTokenizer.h"
#include "TokenRegexSpec.h"
#include "TokenPathSpec.h"
#include "ILogBook.h"
#include "IStreamer.h"

#include <fstream>
#include <sstream>

namespace
{
    using namespace YAM;

    TokenRegexSpec _whiteSpace(R"(^\s+)", "'skip'whitespace", 0);
    TokenRegexSpec _comment(R"(^\/\/.*)", "comment1", 0); // single-line comment
    TokenRegexSpec _nameKey(R"(^name)", "name");
    TokenRegexSpec _dirKey(R"(^dir)", "dir");
    TokenRegexSpec _typeKey(R"(^type)", "type");
    TokenRegexSpec _inputsKey(R"(^inputs)", "inputs");
    TokenRegexSpec _eq(R"(^=)", "=");
    TokenRegexSpec _end(R"(^;)", ";");
    TokenRegexSpec _identifier(R"(^[\w0123456789_-]*)", "identifier");
    TokenPathSpec _path;

    std::string readFile(std::filesystem::path const& path) {
        std::ifstream file(path);
        std::stringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

    // File :== { Repo }*
    // Repo :== RepoName Dir Type [InputRepos] ";'
    // RepoName :== "name" "=" identifier
    // Dir :== "dir" "=" path (relative to home repo or absolute)
    // Type :== "type" "=" Build" | "Track" | "Ignore"
    // InputRepos :== inputs "=" { RepoName }+
    class _Parser
    {
    public:
        _Parser(std::filesystem::path const& path)
            : _tokenizer(path, readFile(path))
        {
            skipWhiteSpace();
            while (!_tokenizer.eos()) {
                parseRepo();
                skipWhiteSpace();
            }
        }
        bool validType(std::string const& type) {
            if (type == "Build") return true;
            if (type == "Track") return true;
            if (type == "Ignore") return true;
            return false;
        }

        void parseRepo() {
            RepositoriesNode::Repo repo;
            consume(&_nameKey);
            consume(&_eq);
            repo.name = consume(&_identifier).value;
            if (_repos.contains(repo.name)) {
                duplicateNameError(repo.name);
            }
            consume(&_dirKey);
            consume(&_eq);
            repo.dir = consume(&_path).value;
            consume(&_typeKey);
            consume(&_eq);
            repo.type = consume(&_identifier).value;
            if (!validType(repo.type)) {
                typeError();
            }
            consume(&_end);
            _repos.insert({ repo.name, repo });
        }

        void skipWhiteSpace() {
            _tokenizer.skip({ &_whiteSpace, &_comment });
        }

        ITokenSpec const* lookAhead(std::vector<ITokenSpec const*> const& specs) {
            skipWhiteSpace();
            _lookAhead = _tokenizer.readNextToken(specs);
            return _lookAhead.spec;
        }

        Token eat(ITokenSpec const* toEat) {
            if (_lookAhead.spec != toEat) {
                syntaxError();
            }
            return _lookAhead;
        }

        Token consume(ITokenSpec const* spec) {
            lookAhead({ spec });
            return eat(spec);
        }

        void duplicateNameError(std::string const& duplicateName) {
            std::stringstream ss;
            ss
                << "Duplicate repository name '" << duplicateName << "' at " 
                << _tokenizer.line() << ", column " << _tokenizer.column()
                << " in file " << _tokenizer.filePath().string()
                << std::endl;
            throw std::runtime_error(ss.str());
        }

        void typeError() {
            std::stringstream ss;
            ss
                << "Repository type at " << _tokenizer.line()
                << ", column " << _tokenizer.column()
                << " in file " << _tokenizer.filePath().string() << std::endl
                << " is invalid." << std::endl;
            ss << "Must be one of Build, Track or Ignore." << std::endl;
            throw std::runtime_error(ss.str());
        }

        void reservedNameError(std::string const& duplicateName) {
            std::stringstream ss;
            ss
                << "Reserved repository name '.' at " << _tokenizer.line()
                << ", column " << _tokenizer.column()
                << " in file " << _tokenizer.filePath().string() << std::endl
                << "'.' is the name of the home repository." << std::endl;
            throw std::runtime_error(ss.str());
        }

        void syntaxError() {
            std::stringstream ss;
            ss
                << "Unexpected token at line " << _tokenizer.line()
                << ", column " << _tokenizer.column()
                << " in file " << _tokenizer.filePath().string()
                << std::endl;
            throw std::runtime_error(ss.str());
        }

        std::map<std::string, RepositoriesNode::Repo> const& repos() const {
            return _repos;
        }

    private:
        BuildFileTokenizer _tokenizer;
        Token _lookAhead;
        std::map<std::string, RepositoriesNode::Repo> _repos;
    };

    FileRepositoryNode::RepoType toType(std::string const& typeStr) {
        if (typeStr == "Build") return FileRepositoryNode::RepoType::Build;
        if (typeStr == "Track") return FileRepositoryNode::RepoType::Track;
        if (typeStr == "Ignore") return FileRepositoryNode::RepoType::Ignore;
        throw std::runtime_error("Unknown repository type");
    }
}

namespace
{
    using namespace YAM;

    void logDuplicateRepo(ILogBook &logBook, FileRepositoryNode const &repo) {
        std::stringstream ss;
        ss 
            << "Cannot add repository " << repo.repoName() << " with directory " << repo.directory()
            << " : repository name is already in use" << std::endl;
        LogRecord e(LogRecord::Error, ss.str());
        logBook.add(e);
    }

    void logSubRepo(ILogBook &logBook, FileRepositoryNode const& repo, FileRepositoryNode const& parent) {
        bool equalDirs = repo.directory() == parent.directory();
        std::stringstream ss;
        if (equalDirs) {
            ss
                << "Cannot add repository " << repo.name() << " with directory " << repo.directory()
                << " : repository directory is equal to directory of repository " << parent.name()
                << std::endl;
        } else {
            ss
                << "Cannot add repository " << repo.name() << " with directory " << repo.directory()
                << " : repository directory is sub-directory of repository "
                << parent.name() << " with directory " << parent.directory()
                << std::endl;
        }
        LogRecord e(LogRecord::Error, ss.str());
        logBook.add(e);
    }

    void logParentRepo(ILogBook &logBook, FileRepositoryNode const& repo, FileRepositoryNode const& sub) {
        std::stringstream ss;
        ss
            << "Cannot add repository " << repo.name() << " with directory " << repo.directory()
            << " : repository directory is parent directory of repository "
            << sub.name() << " with directory " << sub.directory()
            << std::endl;
        LogRecord e(LogRecord::Error, ss.str());
        logBook.add(e);
    }
}

namespace
{
    using namespace YAM;

    uint32_t streamableTypeId = 0;
    static std::shared_ptr<FileRepositoryNode> nullRepo;

    void invalidateRecursively(std::shared_ptr<DirectoryNode> const& dir) {
        if (dir == nullptr) return;
        dir->setState(Node::State::Dirty);
        auto &parser = dir->buildFileParserNode();
        if (parser != nullptr) parser->setState(Node::State::Dirty);
        auto &compiler = dir->buildFileCompilerNode();
        if (compiler != nullptr) compiler->setState(Node::State::Dirty);
        std::vector<std::shared_ptr<DirectoryNode>> subDirs;
        dir->getSubDirs(subDirs);
        for (auto const& subDir : subDirs) {
            invalidateRecursively(subDir);
        }
    }
}

namespace YAM
{
    RepositoriesNode::RepositoriesNode()
        : Node()
        , _modified(false)
    {}

    RepositoriesNode::RepositoriesNode(
        ExecutionContext* context,
        std::shared_ptr<FileRepositoryNode> const& homeRepo
    )
        : Node(context, "repositories")
        , _ignoreConfigFile(true)
        , _configFile(std::make_shared<SourceFileNode>(context, homeRepo->symbolicDirectory()/ RepositoriesNode::configFilePath()))
        , _homeRepo(homeRepo)
        , _configFileHash(rand())
        , _modified(true)
    {
        addRepository(homeRepo);;
        context->nodes().add(_configFile);
        _configFile->addObserver(this);
    }

    void RepositoriesNode::ignoreConfigFile(bool ignore) {
        _ignoreConfigFile = ignore;
    }

    bool RepositoriesNode::ignoreConfigFile() const {
        return _ignoreConfigFile;
    }

    std::filesystem::path RepositoriesNode::absoluteConfigFilePath() const {
        return _configFile->absolutePath();
    }

    std::shared_ptr<FileRepositoryNode> const& RepositoriesNode::homeRepository() const {
        return _homeRepo;
    }

    std::map<std::string, std::shared_ptr<FileRepositoryNode>> const& RepositoriesNode::repositories() const {
        return _repositories;
    }

    std::shared_ptr<FileRepositoryNode> const& RepositoriesNode::findRepository(std::string const& repoName) const {
        auto it = _repositories.find(repoName);
        if (it == _repositories.end()) {
            return nullRepo;
        }
        return it->second;
    }

    std::shared_ptr<FileRepositoryNode> const& RepositoriesNode::findRepositoryContaining(
        std::filesystem::path const& path
    ) const {
        for (auto const& pair : _repositories) {
            auto const& repo = pair.second;
            if (repo->lexicallyContains(path)) return repo;
        }
        return nullRepo;
    }

    bool RepositoriesNode::addRepository(std::shared_ptr<FileRepositoryNode> const& repo) {
        auto it = _repositories.find(repo->repoName());
        if (it != _repositories.end()) {
            logDuplicateRepo(*(context()->logBook()), *repo);
            return false;
        }
        bool ok = true;
        for (auto const& pair : _repositories) {
            auto other = pair.second;
            if (other->lexicallyContains(repo->directory())) {
                logSubRepo(*(context()->logBook()), *repo, *other);
                ok = false;
            }
            if (repo->lexicallyContains(other->directory())) {
                logParentRepo(*(context()->logBook()), *repo, *other);
                ok = false;
            }
        }
        if (ok) {
            modified(true);
            context()->nodes().add(repo);
            _repositories.insert({ repo->repoName(), repo });
            repo->addObserver(this);
        }
        return ok;
    }

    bool RepositoriesNode::removeRepository(std::string const& repoName) {
        auto it = _repositories.find(repoName);
        if (it == _repositories.end()) return false;
        std::shared_ptr<FileRepositoryNode>& repo = it->second;
        modified(true);
        repo->removeObserver(this);
        repo->removeYourself();
        context()->nodes().remove(repo);
        _repositories.erase(it);
        return true;
    }

    void RepositoriesNode::startWatching() {
        for (auto const& pair : _repositories) {
            pair.second->startWatching();
        }
    }

    void RepositoriesNode::stopWatching() {
        for (auto const& pair : _repositories) {
            pair.second->stopWatching();
        }
    }

    void RepositoriesNode::start() {
        Node::start();
        if (_ignoreConfigFile) {
            postCompletion(Node::State::Ok);
        } else {
            std::vector<std::shared_ptr<Node>> requisites{ _configFile };
            auto callback = Delegate<void, Node::State>::CreateLambda(
                [this](Node::State state) { handleRequisitesCompletion(state); }
            );
            Node::startNodes(requisites, callback);
        }
    }

    void RepositoriesNode::handleRequisitesCompletion(Node::State newState) {
        if (newState != Node::State::Ok) {
            notifyCompletion(newState);
        } else if (_configFileHash == _configFile->hashOf(FileAspect::entireFileAspect().name())) {
            notifyCompletion(Node::State::Ok);
        } else {
            std::stringstream ss;
            ss
                << className() << " " << name().string()
                << " reparses " << _configFile->absolutePath()
                << std::endl;
            LogRecord change(LogRecord::FileChanges, ss.str());
            context()->addToLogBook(change);

            _configFileHash = _configFile->hashOf(FileAspect::entireFileAspect().name());
            if (parseAndUpdate()) {
                notifyCompletion(Node::State::Ok);
            } else {
                _configFileHash = rand();
                modified(true);
                notifyCompletion(Node::State::Failed);
            }
        }
    }

    bool RepositoriesNode::parseAndUpdate() {
        try {
            _Parser parser(_configFile->absolutePath());
            return updateRepos(parser.repos());
        } catch (std::runtime_error e) {
            LogRecord error(LogRecord::Aspect::Error, e.what());
            context()->addToLogBook(error);
            return false;
        }
    }

    bool RepositoriesNode::updateRepos(
        std::map<std::string, RepositoriesNode::Repo> const& repos
    ) {
        // add new and update existing repos

        bool ok = true;

        // Find removed repos...
        std::vector<std::shared_ptr<FileRepositoryNode>> toRemove;
        for (auto const& pair : _repositories) {
            std::string frepoName = pair.first;
            auto frepo = pair.second;
            if (frepo != _homeRepo) {
                auto it = repos.find(frepoName);
                if (it == repos.end()) {
                    auto fit = _repositories.find(frepoName);
                    toRemove.push_back(fit->second);
                }
            }
        }
        // ... and remove them
        for (auto const& repo : toRemove) {
            removeRepository(repo->repoName());
            modified(true);
        }

        // Add new repos, update existing ones.
        for (auto const& pair : repos) {
            std::shared_ptr<FileRepositoryNode> frepo;
            Repo const& repo = pair.second;
            std::filesystem::path absRepoDir = repo.dir;
            if (absRepoDir.is_relative()) absRepoDir = (_homeRepo->directory() / repo.dir);
            if (!std::filesystem::is_directory(absRepoDir)) {
                std::stringstream ss;
                ss
                    << "Repository directory " << absRepoDir.string() << " does not exist." << std::endl
                    << "See the definition for the repository named " << repo.name << " in file "
                    << _configFile->absolutePath().string() << std::endl;
                throw std::runtime_error(ss.str());
            }
            absRepoDir = std::filesystem::canonical(absRepoDir);
            auto it = _repositories.find(repo.name);
            if (it == _repositories.end()) {
                frepo = std::make_shared<FileRepositoryNode>(context(), repo.name, absRepoDir);
                ok = addRepository(frepo);
                if (!ok) return false;
            } else {
                frepo = it->second;
            }
            frepo->repoType(toType(repo.type));
            if (!updateRepoDirectory(*frepo, absRepoDir)) return false;
        }

        auto oldHash = _hash;
        _hash = computeHash();
        if (_hash != oldHash) {
            // Invalidate all directory nodes to make sure that
            // nodes that depend on repository properties will
            // re-execute
            for (auto const& pair : _repositories) {
                invalidateRecursively(pair.second->directoryNode());
            }
        }
        return true;
    }

    bool RepositoriesNode::updateRepoDirectory(FileRepositoryNode &frepo, std::filesystem::path const& newDir) {
        if (frepo.directory() != newDir) {
            for (auto const& pair : _repositories) {
                auto other = pair.second;
                if (other.get() == &frepo) continue;
                if (other->lexicallyContains(newDir)) {
                    logSubRepo(*(context()->logBook()), frepo, *other);
                    return false;
                }
                if (frepo.lexicallyContains(other->directory())) {
                    logParentRepo(*(context()->logBook()), frepo, *other);
                    return false;
                }
            }
            frepo.directory(newDir);
        }
        return true;
    }

    XXH64_hash_t RepositoriesNode::computeHash() const {
        std::vector<XXH64_hash_t> hashes;
        for (auto const& pair : _repositories) {
            hashes.push_back(pair.second->hash());
        }
        return XXH64(hashes.data(), sizeof(XXH64_hash_t) * hashes.size(), 0);
    }

    void RepositoriesNode::setStreamableType(uint32_t type) {
        streamableTypeId = type;
    }

    uint32_t RepositoriesNode::typeId() const {
        return streamableTypeId;
    }

    void RepositoriesNode::stream(IStreamer* streamer) {
        Node::stream(streamer);
        streamer->stream(_ignoreConfigFile);
        streamer->stream(_configFile);
        streamer->stream(_homeRepo);
        streamer->streamMap(_repositories);
        streamer->stream(_configFileHash);
    }

    void RepositoriesNode::prepareDeserialize() {
        Node::prepareDeserialize();
        _configFile->removeObserver(this);
        for (auto& pair : _repositories) pair.second->removeObserver(this);
    }

    bool RepositoriesNode::restore(void* context, std::unordered_set<IPersistable const*>& restored) {
        if (!Node::restore(context, restored)) return false;
        _configFile->addObserver(this);
        for (auto& pair : _repositories) pair.second->restore(context, restored);
        for (auto& pair : _repositories) pair.second->addObserver(this);
        _hash = computeHash();
        return true;
    }
}