#include "FileAspect.h"

namespace YAM
{
    FileAspect::FileAspect(
        std::string const & name,
        RegexSet const& fileNamePatterns,
        Delegate<XXH64_hash_t, std::filesystem::path const&> const& hashFunction)
        : _name(name)
        , _fileNamePatterns(fileNamePatterns)
        , _hashFunction(hashFunction)
    { }

    std::string const& FileAspect::name() const {
        return _name;
    }

    RegexSet& FileAspect::fileNamePatterns() {
        return _fileNamePatterns;
    }

    Delegate<XXH64_hash_t, std::filesystem::path const&> const& FileAspect::hashFunction() const {
        return _hashFunction;
    }

    bool FileAspect::appliesTo(std::filesystem::path fileName) const {
        std::string name = fileName.string();
        return _fileNamePatterns.matches(name);
    }

    XXH64_hash_t FileAspect::hash(std::filesystem::path const& fileName) const {
        return _hashFunction.Execute(fileName);
    }

    FileAspect const & FileAspect::entireFileAspect() {
        static Delegate<XXH64_hash_t, std::filesystem::path const&> hashEntireFile =
            Delegate<XXH64_hash_t, std::filesystem::path const&>::CreateLambda(
                [](std::filesystem::path const& fn) {
                    return XXH64_file(fn.string().c_str());
                });
        static FileAspect entireFileAspect(
            std::string("entireFile"), 
            RegexSet({ ".*" }), 
            hashEntireFile);
        return entireFileAspect;
    }
}