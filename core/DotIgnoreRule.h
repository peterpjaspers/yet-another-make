#pragma once

#include "IStreamable.h"
#include "xxhash.h"

#include <filesystem>
#include <string>
#include <regex>

namespace YAM
{
    class DotIgnoreRule : public IStreamable
    {
    public:
        DotIgnoreRule() {} // for streaming

        DotIgnoreRule(std::string const& pattern, std::string const& source);

        bool ignore(std::filesystem::path const& path) const;
        bool match(std::filesystem::path const& path) const;

        std::string const& pattern() const;
        bool negate() const;
        bool isAnchored() const;
        bool isDirOnly() const;

        XXH64_hash_t computeRuleId(XXH64_hash_t seed, std::string const& anchor) const;
        XXH64_hash_t ruleId() const { return _ruleId; }
        void ruleId(XXH64_hash_t id) { _ruleId = id; }

        std::string const& reString() const;
        std::string const& extension() const { return _extension.string(); }
        std::regex const& re() const;

        static void streamVector(
            IStreamer* streamer,
            std::vector<DotIgnoreRule>& rules
        );

        uint32_t typeId() const override { throw "not supported"; }
        void stream(IStreamer* streamer) override;

    private:
        void parse(std::string const& source);

        std::string _pattern;
        bool _negate;
        bool _anchored;
        bool _dirOnly;
        std::filesystem::path _extension;
        XXH64_hash_t _ruleId;
        std::string _reString;
        std::regex _re;
    };
}
