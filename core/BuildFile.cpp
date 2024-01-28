#include "BuildFile.h"

namespace
{
    using namespace YAM;

    uint32_t ruleType = 32;
    //uint32_t varType = 33;

    template<typename T>
    void streamVector(IStreamer* streamer, std::vector<T>& items) {
        uint32_t nItems;
        if (streamer->writing()) {
            const std::size_t length = items.size();
            if (length > UINT_MAX) throw std::exception("vector too large");
            nItems = static_cast<uint32_t>(length);
        }
        streamer->stream(nItems);
        if (streamer->writing()) {
            for (std::size_t i = 0; i < nItems; ++i) {
                items[i].stream(streamer);
            }
        } else {
            T item;
            for (std::size_t i = 0; i < nItems; ++i) {
                item.stream(streamer);
                items.push_back(item);
            }
        }
    }

    void writeNode(IStreamer* streamer, std::shared_ptr<BuildFile::Node> node) {
        uint32_t tid = node->typeId();
        streamer->stream(tid);
        node->stream(streamer);
    }

    void readNode(IStreamer* streamer, std::shared_ptr<BuildFile::Node>& node) {
        uint32_t tid;
        streamer->stream(tid);
        if (tid == ruleType) {
            node = std::make_shared<BuildFile::Rule>();
            node->stream(streamer);
        } else {
            throw std::runtime_error("unsupported node type");
        }
    }

    void streamNodes(IStreamer* streamer, std::vector<std::shared_ptr<BuildFile::Node>>& nodes) {
        uint32_t nItems;
        if (streamer->writing()) {
            const std::size_t length = nodes.size();
            if (length > UINT_MAX) throw std::exception("vector too large");
            nItems = static_cast<uint32_t>(length);
        }
        streamer->stream(nItems);
        if (streamer->writing()) {
            for (std::size_t i = 0; i < nItems; ++i) {
                writeNode(streamer, nodes[i]);
            }
        } else {
            for (uint32_t i = 0; i < nItems; ++i) {
                std::shared_ptr<BuildFile::Node> node;
                readNode(streamer, node);
                nodes.push_back(node);
            }
        }
    }
}

namespace YAM {namespace BuildFile {

    Node::Node() : line(0), column(0) {}
    Node::~Node() {}

    void Node::addHashes(std::vector<XXH64_hash_t>& hashes) {
        hashes.push_back(line);
        hashes.push_back(column);
    }

    void Node::stream(IStreamer* streamer) {
        streamer->stream(line);
        streamer->stream(column);
    }

    void Input::addHashes(std::vector<XXH64_hash_t>& hashes) {
        Node::addHashes(hashes);
        hashes.push_back(exclude);
        hashes.push_back(XXH64_string(path.string()));
        hashes.push_back(static_cast<uint16_t>(pathType));
    }

    void Input::stream(IStreamer* streamer) {
        Node::stream(streamer);
        streamer->stream(exclude);
        streamer->stream(path);
        uint16_t ptype;
        if (streamer->writing()) ptype = pathType;
        streamer->stream(ptype);
        if (streamer->reading()) pathType = static_cast<PathType>(ptype);
    }
    
    void Inputs::addHashes(std::vector<XXH64_hash_t>& hashes) {
        Node::addHashes(hashes);
        for (auto& input : inputs) {
            input.addHashes(hashes);
        }
    }

    void Inputs::stream(IStreamer* streamer) {
        Node::stream(streamer);
        uint32_t length = static_cast<uint32_t>(inputs.size());
        streamVector(streamer, inputs);
    }

    void Script::addHashes(std::vector<XXH64_hash_t>& hashes) {
        Node::addHashes(hashes);
        hashes.push_back(XXH64_string(script));
    }

    void Script::stream(IStreamer* streamer) {
        Node::stream(streamer);
        streamer->stream(script);
    }

    void Output::addHashes(std::vector<XXH64_hash_t>& hashes) {
        Node::addHashes(hashes);
        hashes.push_back(ignore);
        hashes.push_back(XXH64_string(path.string()));
        hashes.push_back(static_cast<uint16_t>(pathType));
    }

    void Output::stream(IStreamer* streamer) {
        Node::stream(streamer);
        streamer->stream(ignore);
        streamer->stream(path);
        uint16_t ptype;
        if (streamer->writing()) ptype = pathType;
        streamer->stream(ptype);
        if (streamer->reading()) pathType = static_cast<PathType>(ptype);
    }

    void Outputs::addHashes(std::vector<XXH64_hash_t>& hashes) {
        Node::addHashes(hashes);
        for (auto& output : outputs) {
            output.addHashes(hashes);
        }
    }

    void Outputs::stream(IStreamer* streamer) {
        Node::stream(streamer);
        streamVector(streamer, outputs);
    }

    void Rule::addHashes(std::vector<XXH64_hash_t>& hashes) {
        Node::addHashes(hashes);
        hashes.push_back(forEach);
        cmdInputs.addHashes(hashes);
        orderOnlyInputs.addHashes(hashes);
        script.addHashes(hashes);
        outputs.addHashes(hashes);
        for (auto const& grp : outputGroups) hashes.push_back(XXH64_string(grp.string()));
        for (auto const& bin : bins) hashes.push_back(XXH64_string(bin.string()));
    }

    uint32_t Rule::typeId() const { return ruleType; }

    void Rule::stream(IStreamer* streamer) {
        Node::stream(streamer);
        streamer->stream(forEach);
        cmdInputs.stream(streamer);
        orderOnlyInputs.stream(streamer);
        script.stream(streamer);
        outputs.stream(streamer);
        streamer->streamVector(outputGroups);
        streamer->streamVector(bins);
    }

    void Deps::addHashes(std::vector<XXH64_hash_t>& hashes) {
        Node::addHashes(hashes);
        for (auto const& path : depBuildFiles) {
            hashes.push_back(XXH64_string(path.string()));
        }
        for (auto const& path : depGlobs) {
            hashes.push_back(XXH64_string(path.string()));
        }
    }

    void Deps::stream(IStreamer* streamer) {
        Node::stream(streamer);
        streamer->streamVector(depBuildFiles);
        streamer->streamVector(depGlobs);
    }

    XXH64_hash_t File::computeHash() {
        std::vector<XXH64_hash_t> hashes;
        hashes.push_back(XXH64_string(buildFile.string()));
        Node::addHashes(hashes);
        deps.addHashes(hashes);
        for (auto& varOrRule : variablesAndRules) {
            varOrRule->addHashes(hashes);
        }
        return XXH64(hashes.data(), sizeof(XXH64_hash_t) * hashes.size(), 0);
    }

    void File::stream(IStreamer* streamer) {
        Node::stream(streamer);
        streamer->stream(buildFile);
        deps.stream(streamer);
        streamNodes(streamer, variablesAndRules);
    }
}}