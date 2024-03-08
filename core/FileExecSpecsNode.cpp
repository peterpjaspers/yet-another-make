#include "FileExecSpecsNode.h"
#include "ExecutionContext.h"
#include "SourceFileNode.h"
#include "BuildFileTokenizer.h"
#include "TokenRegexSpec.h"
#include "LogRecord.h"
#include "IStreamer.h"

#include <fstream>

namespace
{
    using namespace YAM;

    uint32_t streamableTypeId = 0;

    TokenRegexSpec _whiteSpace(R"(^\s+)", "'skip'whitespace", 0);
    TokenRegexSpec _comment(R"(^\/\/.*)", "comment1", 0); // single-line comment
    TokenRegexSpec _ext(R"(^\.\w+)", "stem");
    TokenRegexSpec _arrow(R"(^=>)", "arrow");
    TokenRegexSpec _fmt(R"(^.*$)", "fmt");

    std::string readFile(std::filesystem::path const& path) {
        std::ifstream file(path);
        std::stringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

    class Parser
    {
    public:
        Parser(std::filesystem::path const& path)
            : _tokenizer(path, readFile(path))
        {
            lookAhead({ &_ext });
            while (_lookAhead.spec == &_ext) {
                parseExecSpec();
                lookAhead({ &_ext });
            }
        }

        void parseExecSpec() {
            Token stem = eat(&_ext);
            lookAhead({ &_arrow });
            eat(&_arrow);
            lookAhead({ &_fmt });
            Token fmt = eat(&_fmt);
            _execSpecs.insert({ stem.value, fmt.value });
        }

        void lookAhead(std::vector<ITokenSpec const*> const& specs) {
            _tokenizer.skip({ &_whiteSpace, &_comment });
            _lookAhead = _tokenizer.readNextToken(specs);
        }

        Token eat(ITokenSpec const* toEat) {
            if (_lookAhead.spec != toEat) {
                syntaxError();
            }
            return _lookAhead;
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

        std::map<std::filesystem::path, std::string> const& invokations() const {
            return _execSpecs;
        }

    private:
        BuildFileTokenizer _tokenizer;
        Token _lookAhead;
        std::map<std::filesystem::path, std::string> _execSpecs;
    };

    std::string replace(std::string fmt, std::string const& buildFile) {
        std::string result;
        bool pct = false;
        for (char c : fmt) {
            switch (c) {
            case '%':
            {
                if (pct) {
                    result.push_back('%');
                    result.push_back('%');
                    pct = false;
                } else {
                    pct = true;
                }
                break;
            }
            case 'f':
            {
                if (pct) {
                    result.append(buildFile);
                    pct = false;
                } else {
                    result.push_back(c);
                }
                break;
            }
            default:
                pct = false;
                result.push_back(c);
            }
        }
        return result;
    }
}

namespace YAM
{
    FileExecSpecsNode::FileExecSpecsNode() {}

    FileExecSpecsNode::FileExecSpecsNode(
        ExecutionContext* context, 
        std::filesystem::path const& repoName)
        : Node(context, repoName / "__invokeConfig")
        , _configFile(std::make_shared<SourceFileNode>(context, repoName / configFilePath()))
        , _executionHash(rand())
    {
        context->nodes().add(_configFile);
        _configFile->addObserver(this);
    }

    void FileExecSpecsNode::cleanup() {
        _configFile->removeObserver(this);
        _configFile = nullptr;
    }

    std::filesystem::path FileExecSpecsNode::absoluteConfigFilePath() const {
        return _configFile->absolutePath();
    }

    std::shared_ptr<SourceFileNode> FileExecSpecsNode::configFileNode() const {
        return _configFile;
    }

    std::string FileExecSpecsNode::command(std::filesystem::path const& fileName) const {
        std::filesystem::path ext = fileName.extension();
        auto it = _commandFmts.find(ext);
        if (it == _commandFmts.end()) return "";
        std::string const& fmt = it->second;
        std::string invokation = replace(fmt, fileName.string());
        return invokation;
    }

    void FileExecSpecsNode::start() {
        Node::start();
        std::vector<std::shared_ptr<Node>> requisites{ _configFile };
        auto callback = Delegate<void, Node::State>::CreateLambda(
            [this](Node::State state) { handleRequisitesCompletion(state); }
        );
        Node::startNodes(requisites, callback);
    }

    void FileExecSpecsNode::handleRequisitesCompletion(Node::State newState) {
        if (newState != Node::State::Ok) {
            notifyCompletion(newState);
        } else {
            if (parse()) {
                notifyCompletion(Node::State::Ok);
            } else {
                notifyCompletion(Node::State::Failed);
            }
        }
    }

    bool FileExecSpecsNode::parse() {
        try {
            Parser parser(_configFile->absolutePath());
            _commandFmts = parser.invokations();
            _executionHash = computeExecutionHash();
            return true;
        } catch (std::runtime_error e) {
            LogRecord error(LogRecord::Aspect::Error, e.what());
            context()->addToLogBook(error);
            return false;
        }
    }

    XXH64_hash_t FileExecSpecsNode::computeExecutionHash() const {
        std::vector<XXH64_hash_t> hashes;
        for (auto const& pair : _commandFmts) {
            hashes.push_back(XXH64_string(pair.first.string()));
            hashes.push_back(XXH64_string(pair.second));
        }
        XXH64_hash_t hash = XXH64(hashes.data(), sizeof(XXH64_hash_t) * hashes.size(), 0);
        return hash;
    }

    void FileExecSpecsNode::setStreamableType(uint32_t type) {
        streamableTypeId = type;
    }

    uint32_t FileExecSpecsNode::typeId() const {
        return streamableTypeId;
    }

    void FileExecSpecsNode::stream(IStreamer* streamer) {
        Node::stream(streamer);
        if (state() != Node::State::Deleted) {
            streamer->stream(_configFile);
            streamer->streamMap(_commandFmts);
            streamer->stream(_executionHash);
        }
    }

    void FileExecSpecsNode::prepareDeserialize() {
        Node::prepareDeserialize();
        if (state() != Node::State::Deleted) {
            _commandFmts.clear();
            _configFile->removeObserver(this);
        }
    }

    bool FileExecSpecsNode::restore(void* context, std::unordered_set<IPersistable const*>& restored) {
        if (!Node::restore(context, restored)) return false;
        if (state() != Node::State::Deleted) {
            _configFile->addObserver(this);
        }
        return true;
    }
}