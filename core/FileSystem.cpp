#include "FileSystem.h"

#include <mutex>
#include <algorithm>
#include <cwctype>

namespace
{
    std::mutex mutex; // to guard access to std::tmpnam
}

namespace YAM
{
    std::filesystem::path FileSystem::createUniqueDirectory(std::string const& postfix) {
        std::filesystem::path d = uniquePath(postfix);
        while (!std::filesystem::create_directory(d)) {
            d = uniquePath(postfix);
        }
        std::filesystem::create_directory(d);
        return canonicalPath(d);
    }

    std::filesystem::path FileSystem::uniquePath(std::string const& postfix) {
        static std::string prefix("yam_");
        std::lock_guard<std::mutex> lock(mutex);
        char* name = std::tmpnam(nullptr);
        if (name == nullptr) throw std::exception("std::tmpnam failed");
        std::filesystem::path p(name);
        std::filesystem::path up(p.parent_path() / (prefix + p.filename().string() + postfix ));
        return up;
    }

    std::filesystem::path FileSystem::canonicalPath(std::filesystem::path const& path) {
        std::error_code ec;
        std::filesystem::path nPath = std::filesystem::canonical(path, ec);
        if (ec.value() != 0) {
            auto msg = ec.message();
            nPath = path;
        }
        return nPath;
    }

    std::filesystem::path FileSystem::toLower(std::filesystem::path const& path) {
        std::string lower = path.string();
        std::transform(
            lower.begin(), lower.end(),
            lower.begin(),
            [](unsigned char c) { return std::tolower(c); });
        return std::filesystem::path(lower);
    }
}