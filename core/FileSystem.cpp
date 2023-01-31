#include "FileSystem.h"

#include <mutex>
#include <Windows.h>
#include <stdio.h>
#include <fileapi.h>
#include <tchar.h>
#include <algorithm>
#include <cwctype>

namespace
{	
	bool isShortPath(std::wstring const& path) {
		auto it = std::find(path.begin(), path.end(), '~');
	    return it != path.end();
	}

	std::wstring normalize(std::wstring const& path) {
		std::wstring nPath;
		if (!isShortPath(path)) {
			nPath = path;
		} else {
			wchar_t buf[MAX_PATH];
			DWORD length = GetLongPathNameW(
				(LPCWSTR)(path.c_str()),
				(LPWSTR)(&buf),
				MAX_PATH);
			if (length > MAX_PATH) throw std::exception("path too long");
			if (length == 0) {
				nPath = std::wstring(path);
			} else {
				nPath = std::wstring(buf, length);
			}
		}
		std::transform(nPath.begin(), nPath.end(), nPath.begin(), std::towlower);
		return nPath;
	}
	
	std::mutex mutex; // to guard access to std::tmpnam
}

namespace YAM
{
	std::filesystem::path FileSystem::createUniqueDirectory() {
		std::filesystem::path d = uniquePath();
		while (!std::filesystem::create_directory(d)) {
			d = uniquePath();
		}
		return normalizePath(d);
	}

	std::filesystem::path FileSystem::uniquePath() {
		std::lock_guard<std::mutex> lock(mutex);
		char* name = std::tmpnam(nullptr);
		if (name == nullptr) throw std::exception("std::tmpnam failed");
		return std::filesystem::path(name);
	}

	std::filesystem::path FileSystem::normalizePath(std::wstring const& path) {
		std::wstring p = normalize(path);
		return std::filesystem::path(p);
	}

	std::filesystem::path FileSystem::normalizePath(std::filesystem::path const& path) {
		return normalizePath(path.wstring());
	}
}