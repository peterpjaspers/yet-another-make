#pragma once

#include "Node.h"
#include "../xxHash/xxhash.h"

#include <map>
#include <tuple>

namespace YAM
{
    class __declspec(dllexport) FileNode : public Node
    {
    public:
        FileNode(ExecutionContext* context, std::filesystem::path name);

        // Inherited via Node
        virtual bool supportsPrerequisites() const override;
        virtual void appendPrerequisites(std::vector<Node*>& prerequisites) const override;

        virtual bool supportsOutputs() const override;
        virtual void appendOutputs(std::vector<Node*>& outputs) const override;

        virtual bool supportsInputs() const override;
        virtual void appendInputs(std::vector<Node*>& inputs) const override;

        // Set the aspect hashers to be used to compute hashes of file aspects.
        // Pre: state() != State::Executing
        typedef Delegate<XXH64_hash_t, std::filesystem::path const &> HashFunc;
        struct AspectHasher { std::string name; HashFunc hashFunction; };
        void setAspectHashers(std::vector<AspectHasher> const& newHashers);

        // Return the hash of the given aspect.
        // Pre: state() == State::Ok
        // Pre: aspects.contains(aspectName)
        XXH64_hash_t hashOf(std::string const& aspectName);

        // Re-compute the aspect hashes
        void rehash();

    protected:
        // Inherited via Node
        virtual bool pendingStartSelf() const override;
        virtual void startSelf() override;

    private:
        void execute();

        // file modification time at time of last rehash
        std::chrono::time_point<std::chrono::utc_clock> _lastWriteTime;
        std::vector<AspectHasher> _hashers;
        // aspect name => hash value
        std::map<std::string, XXH64_hash_t> _hashes;
    };
}

