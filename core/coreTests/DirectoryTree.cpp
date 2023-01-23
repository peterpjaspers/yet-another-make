#pragma once
#include "gtest/gtest.h"
#include "DirectoryTree.h"
#include "../DirectoryNode.h"
#include "../SourceFileNode.h"
#include "../FileNode.h"
#include "../RegexSet.h"

namespace YAMTest
{
    DirectoryTree::DirectoryTree(
        std::filesystem::path const& dirName, 
        unsigned int nLevels,
        YAM::RegexSet const& excludes)
        : _path(dirName)
        , _nLevels(nLevels)
        , _excludes(excludes)
    {
        std::filesystem::create_directories(dirName);

        std::filesystem::path file1(_path / "File1");
        std::filesystem::path file2(_path / "File2");
        std::filesystem::path file3(_path / "File3");
        _files.push_back(file1);
        _files.push_back(file2);
        _files.push_back(file3);

        for (auto f : _files) std::ofstream{ f };

        if (_nLevels > 0) {
            std::filesystem::path dir1(_path / "SubDir1");
            std::filesystem::path dir2(_path / "SubDir2");
            std::filesystem::path dir3(_path / "SubDir3");
            _subDirs.push_back(new DirectoryTree(dir1, nLevels - 1, _excludes));
            _subDirs.push_back(new DirectoryTree(dir2, nLevels - 1, _excludes));
            _subDirs.push_back(new DirectoryTree(dir3, nLevels - 1, _excludes));
        }

        updateHash();
    }

    DirectoryTree::~DirectoryTree() {
        for (auto f : _files) std::filesystem::remove(f);
        for (auto d : _subDirs) delete d;
        std::filesystem::remove_all(_path);
    }

    // add a file at specified level in tree
    void DirectoryTree::addFile() {
        char buf[32];
        const std::string nextIndex(_itoa((int)(_files.size() + 1), buf, 10));
        const std::string nextName("File" + nextIndex);
        const std::filesystem::path nextPath(_path / nextName);
        _files.push_back(nextPath);
        std::ofstream{ nextPath };
        updateHash();
    }

    void DirectoryTree::addDirectory() {
        if (_nLevels == 0) _nLevels = 1;
        char buf[32];
        const std::string nextIndex(_itoa((int)(_subDirs.size() + 1), buf, 10));
        const std::string nextName("SubDir" + nextIndex);
        const std::filesystem::path nextPath(_path / nextName);
        _subDirs.push_back(new DirectoryTree(nextPath, _nLevels - 1, _excludes));
        std::ofstream{ nextPath };
        updateHash();
    }

    void DirectoryTree::modifyFile(std::string const& fileName) {
        std::filesystem::path path(_path / fileName);
        std::ofstream file(path);
        file.write(fileName.c_str(), fileName.length());
        file.close();
    }

    void DirectoryTree::deleteFile(std::string const& fileName) {
        std::filesystem::path path(_path / fileName);
        std::filesystem::remove(path);
    }

    void DirectoryTree::renameFile(std::string const& fileName, std::string const& newFileName) {
        std::filesystem::path oldPath(_path / fileName);
        std::filesystem::path newPath(_path / newFileName);
        std::filesystem::rename(oldPath, newPath);
        auto it = std::find(_files.begin(), _files.end(), oldPath);
        if (it != _files.end()) {
            _files.erase(it);
            _files.push_back(newFileName);
        }
    }

    std::filesystem::path const& DirectoryTree::path() const { return _path; }
    unsigned int DirectoryTree::nLevels() const { return _nLevels; }

    std::vector<std::filesystem::path> DirectoryTree::getFiles() const {
        std::vector<std::filesystem::path> files;
        for (auto f : _files) {
            if (!_excludes.matches(f.string())) {
                files.push_back(f);
            }
        }
        return files;
    }

    std::vector<DirectoryTree*> DirectoryTree::getSubDirs() const {
        std::vector<DirectoryTree*> dirs;
        for (auto d : _subDirs) {
            if (!_excludes.matches(d->path().string())) {
                dirs.push_back(d);
            }
        }
        return dirs;
    }

    XXH64_hash_t DirectoryTree::getHash() const { return _hash; }

    void DirectoryTree::updateHash() {
        std::vector<XXH64_hash_t> hashes;
        for (auto const& f : _files)
        {
            if (!_excludes.matches(f.string())) {
                hashes.push_back(XXH64_string(f.string()));
            }
        }
        for (auto d : _subDirs) {
            if (!_excludes.matches(d->path().string())) {
                hashes.push_back(XXH64_string(d->path().string()));
            }
        }
        _hash = XXH64(hashes.data(), sizeof(XXH64_hash_t) * hashes.size(), 0);
    }
    
    void verify(DirectoryTree* expected, YAM::DirectoryNode* actual) {

        EXPECT_EQ(YAM::Node::State::Ok, actual->state());
        EXPECT_EQ(expected->getHash(), actual->getHash());

        std::vector<std::shared_ptr<YAM::FileNode>> fileNodes;
        actual->getFiles(fileNodes);
        EXPECT_EQ(expected->getFiles().size(), fileNodes.size());
        for (std::size_t i = 0; i < fileNodes.size(); ++i) {
            EXPECT_EQ(expected->getFiles()[i], fileNodes[i]->name());
        }

        std::vector<std::shared_ptr<YAM::DirectoryNode>> subDirNodes;
        actual->getSubDirs(subDirNodes);
        EXPECT_EQ(expected->getSubDirs().size(), subDirNodes.size());
        for (std::size_t i = 0; i < subDirNodes.size(); ++i) {
            EXPECT_EQ(expected->getSubDirs()[i]->path(), subDirNodes[i]->name());
        }

        std::map<std::filesystem::path, std::shared_ptr<YAM::Node>> const& cNodes = actual->getContent();
        EXPECT_EQ(expected->getFiles().size() + expected->getSubDirs().size(), cNodes.size());
        for (auto f : fileNodes) EXPECT_TRUE(cNodes.contains(f->name()));
        for (auto d : subDirNodes) EXPECT_TRUE(cNodes.contains(d->name()));

        for (std::size_t i = 0; i < subDirNodes.size(); ++i) {
            verify(expected->getSubDirs()[i], subDirNodes[i].get());
        }
    }
}
