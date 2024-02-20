#include "RepositoriesNode.h"
#include "SourceFileNode.h"
#include "ExecutionContext.h"
#include "FileRepository.h"
#include "BuildFileTokenizer.h"
#include "TokenRegexSpec.h"
#include "TokenPathSpec.h"
#include "LogRecord.h"
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
    TokenRegexSpec _identifier(R"(^\w+)", "identifier");
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

        void parseRepo() {
            RepositoriesNode::Repo repo;
            consume(&_nameKey);
            consume(&_eq);
            repo.name = consume(&_identifier).value;
            if (_repos.contains(repo.name)) {
                duplicateNameError(repo.name);
            }
            if (repo.name == ".") {
                reservedNameError(repo.name);
            }
            consume(&_dirKey);
            consume(&_eq);
            repo.dir = consume(&_path).value;
            consume(&_typeKey);
            consume(&_eq);
            Token type = consume(&_identifier);
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

namespace // detect cycles
{
    std::string trailToString(std::list<std::string> const& trail) {
        std::stringstream ss;
        auto end = std::prev(trail.end());
        auto it = trail.begin();
        for (; it != end; ++it) {
            ss << (*it) << " => ";
        }
        ss << (*it) << std::endl;
        return ss.str();
    }
}

namespace
{
    uint32_t streamableTypeId = 0;
    static std::shared_ptr<FileRepository> nullRepo;

    std::filesystem::path configFileSymPath(std::string const& homeRepoName) {
        std::filesystem::path symPath("@@" + homeRepoName);
        symPath /= RepositoriesNode::configFilePath();
        return symPath;
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
        std::shared_ptr<FileRepository> const& homeRepo
    )
        : Node(context, homeRepo->name())
        , _ignoreConfigFile(true)
        , _configFile(std::make_shared<SourceFileNode>(context, configFileSymPath(homeRepo->name())))
        , _configFileHash(rand())
        , _modified(true)
    {
        _repositories.insert({ homeRepo->name(), homeRepo });
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

    std::shared_ptr<FileRepository> RepositoriesNode::homeRepository() const {
        auto it = _repositories.find(name().string());
        if (it != _repositories.end()) return it->second;
        return nullRepo;
    }

    std::map<std::string, std::shared_ptr<FileRepository>> const& RepositoriesNode::repositories() const {
        return _repositories;
    }

    std::shared_ptr<FileRepository> const& RepositoriesNode::findRepository(std::string const& repoName) const {
        auto it = _repositories.find(repoName);
        if (it == _repositories.end()) {
            return nullRepo;
        }
        return it->second;
    }

    bool RepositoriesNode::addRepository(std::shared_ptr<FileRepository> const& repo) {
        auto it = _repositories.find(repo->name());
        if (it != _repositories.end()) return false;
        modified(true);
        _repositories.insert({ repo->name(), repo });
        return true;
    }

    bool RepositoriesNode::removeRepository(std::string const& repoName) {
        auto it = _repositories.find(repoName);
        if (it == _repositories.end()) return false;
        modified(true);
        it->second->clear();
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
            updateRepos(parser.repos());
            modified(true); 
            return true;
        } catch (std::runtime_error e) {
            LogRecord error(LogRecord::Aspect::Error, e.what());
            context()->addToLogBook(error);
            return false;
        }
    }
    void RepositoriesNode::updateRepos(
        std::map<std::string, RepositoriesNode::Repo> const& repos
    ) {
        // add new and update existing repos
        for (auto const& pair : repos) {
            std::shared_ptr<FileRepository> frepo;
            Repo const& repo = pair.second;
            auto it = _repositories.find(repo.name);
            if (it == _repositories.end()) {
                frepo = std::make_shared<FileRepository>(
                    repo.name, repo.dir, context(), true);
                _repositories.insert({ repo.name, frepo });
            } else {
                frepo = it->second;
            }
            frepo->directory(repo.dir);
            //frepo->type(repo.type);
            frepo->inputRepoNames(repo.inputs);
        }
        
        // Find removed repos
        std::string homeRepoName = name().string();
        std::vector<std::shared_ptr<FileRepository>> toRemove;
        for (auto const& pair : _repositories) {
            std::string frepoName = pair.first;
            if (frepoName != homeRepoName) {
                auto it = repos.find(frepoName);
                if (it == repos.end()) {
                    auto fit = _repositories.find(frepoName);
                    toRemove.push_back(fit->second);
                }
            }
        }
        for (auto const& repo : toRemove) {
            removeRepository(repo->name());
        }
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