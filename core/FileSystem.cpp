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
    std::filesystem::path FileSystem::createUniqueDirectory() {
        std::filesystem::path d = uniquePath();
        while (!std::filesystem::create_directory(d)) {
            d = uniquePath();
        }
        auto c = std::filesystem::canonical(d);
        return normalizePath(d);
    }

    std::filesystem::path FileSystem::uniquePath() {
        static std::string prefix("yam_");
        std::lock_guard<std::mutex> lock(mutex);
        char* name = std::tmpnam(nullptr);
        if (name == nullptr) throw std::exception("std::tmpnam failed");
        std::filesystem::path p(name);
        std::filesystem::path up(p.parent_path() / (prefix + p.filename().string()));
        return up;
    }

    std::filesystem::path FileSystem::normalizePath(std::filesystem::path const& path) {
        std::error_code ec;
        std::filesystem::path nPath = std::filesystem::canonical(path, ec);
        if (ec.value() != 0) {
            nPath = path;
        }
#if defined( _WIN32 )
        std::wstring str = nPath.wstring();
        std::transform(str.begin(), str.end(), str.begin(), std::towlower);
        nPath = str;
#endif
        return nPath;
    }
}