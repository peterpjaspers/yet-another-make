#include "FileSystem.h"

#include <mutex>
#include <algorithm>
#include <cwctype>

namespace
{
    std::mutex mutex; // to guard access to std::tmpnam

    std::filesystem::path _yamTempFolder = std::filesystem::temp_directory_path() / "yam_temp";
}

namespace YAM
{
    std::filesystem::path FileSystem::yamTempFolder() {
        return _yamTempFolder;
    }

    std::filesystem::path FileSystem::createUniqueDirectory(std::string const& prefix) {
        std::filesystem::path d = uniquePath(prefix);
        while (!std::filesystem::create_directories(d)) {
            d = uniquePath(prefix);
        }
        std::filesystem::create_directory(d);
        return canonicalPath(d);
    }

    std::filesystem::path FileSystem::uniquePath(std::string const& prefix) {
        std::lock_guard<std::mutex> lock(mutex);
        char* name = std::tmpnam(nullptr);
        if (name == nullptr) throw std::exception("std::tmpnam failed");
        std::filesystem::path p(name);
        std::filesystem::path up(_yamTempFolder / (prefix + p.filename().string()));
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