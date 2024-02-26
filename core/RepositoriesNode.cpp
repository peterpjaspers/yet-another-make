#include "RepositoriesNode.h"
#include "SourceFileNode.h"
#include "ExecutionContext.h"
#include "FileRepositoryNode.h"
#include "BuildFileTokenizer.h"
#include "TokenRegexSpec.h"
#include "TokenPathSpec.h"
#include "ILogBook.h"
#include "IStreamer.h"

#include <fstream>
#include <sstream>

namespace //parser
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
    // Type :== "type" "=" Integrated" | "Coupled" | "Tracked" | "Ignored"
    // InputRepos :== inputs "=" { RepoName }+
    class Parser
    {
    public:
        Parser(std::filesystem::path const& path)
            : _tokenizer(path, readFile(path))
        {
            skipWhiteSpace();
            while (!_tokenizer.eos()) {
                parseRepo();
                skipWhiteSpace();
            }
        }
        bool validType(std::string const& type) {
            if (type == "Integrated") return true;
            if (type == "Coupled") return true;
            if (type == "Tracked") return true;
            if (type == "Ignored") return true;
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
            if (repo.dir.is_absolute()) {
                absDirError();
            }
            consume(&_typeKey);
            consume(&_eq);
            Token type = consume(&_identifier);
            if (!validType(type.value)) {
                typeError();
            }
            if (lookAhead({ &_inputsKey }) == &_inputsKey) {
                eat(&_inputsKey);
                consume(&_eq);
                while (lookAhead({ &_identifier, &_end }) == &_identifier) {
                    std::string inputName = eat(&_identifier).value;
                    repo.inputs.push_back(inputName);
                }
                eat(&_end);
            } else {
                consume(&_end);
            }
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
            ss << "Must be one of Integrated, Coupled, Tracked or Ignored." << std::endl;
            throw std::runtime_error(ss.str());
        }

        void absDirError() {
            std::stringstream ss;
            ss
                << "Repository directory at " << _tokenizer.line()
                << ", column " << _tokenizer.column()
                << " in file " << _tokenizer.filePath().string() << std::endl
                << " must be a path relative to the home repository." << std::endl;
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
    uint32_t streamableTypeId = 0;
    static std::shared_ptr<FileRepositoryNode> nullRepo;
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
        context->nodes().add(homeRepo);
        _repositories.insert({ homeRepo->repoName(), homeRepo });
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
        }
        return ok;
    }

    bool RepositoriesNode::removeRepository(std::string const& repoName) {
        auto it = _repositories.find(repoName);
        if (it == _repositories.end()) return false;
        std::shared_ptr<FileRepositoryNode>& repo = it->second;
        modified(true);
        repo->clear();
        context()->nodes().remove(it->second);
        _repositories.erase(it);
        return true;
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
            Parser parser(_configFile->absolutePath());
            //detectCycles(context(), parser.repos());
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
        for (auto const& pair : repos) {
            std::shared_ptr<FileRepositoryNode> frepo;
            Repo const& repo = pair.second;
            std::filesystem::path absRepoDir = repo.dir;
            if (absRepoDir.is_relative()) absRepoDir = (_homeRepo->directory() / repo.dir);
            if (!std::filesystem::is_directory(absRepoDir)) {
                throw std::runtime_error("No such directory: " + absRepoDir.string());
            }
            absRepoDir = std::filesystem::canonical(absRepoDir);
            auto it = _repositories.find(repo.name);
            if (it == _repositories.end()) {
                frepo = std::make_shared<FileRepositoryNode>(context(), repo.name, absRepoDir, true);
                ok = addRepository(frepo);
                if (!ok) return false;
                modified(true);
            } else {
                frepo = it->second;
            }
            if (!updateRepoDirectory(*frepo, absRepoDir)) return false;
            //frepo->type(repo.type);
            frepo->inputRepoNames(repo.inputs);
        }
        
        // Find removed repos
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
        for (auto const& repo : toRemove) {
            removeRepository(repo->repoName());
            modified(true);
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
            frepo.modified(true);
        }
        return true;
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
    }

    bool RepositoriesNode::restore(void* context, std::unordered_set<IPersistable const*>& restored) {
        if (!Node::restore(context, restored)) return false;
        _configFile->addObserver(this);
        for (auto& pair : _repositories) pair.second->restore(context, restored);
        return true;
    }
}