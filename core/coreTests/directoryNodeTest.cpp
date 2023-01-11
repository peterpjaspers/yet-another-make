#include "gtest/gtest.h"
#include "executeNode.h"
#include "../DirectoryNode.h"
#include "../FileNode.h"
#include "../ExecutionContext.h"
#include "../../xxhash/xxhash.h"

#include <chrono>
#include <fstream>
#include <cstdio>

namespace
{
    using namespace YAM;

    std::chrono::seconds timeout(10);

    class TestDirectoryTree
    {
    public:
        TestDirectoryTree(std::filesystem::path const& dirName, unsigned int nLevels)
            : _path(dirName)
            , _nLevels(nLevels)
        {
            std::filesystem::create_directories(dirName);

            _files.push_back(_path / "FileA.cpp");
            _files.push_back(_path / "FileB.cpp");
            _files.push_back(_path / "FileC.cpp");
            for (auto f : _files) std::ofstream{ f };

            if (_nLevels > 0) {
                _subDirs.push_back(new TestDirectoryTree(_path / "SubDirA", nLevels - 1));
                _subDirs.push_back(new TestDirectoryTree(_path / "SubDirB", nLevels - 1));
                _subDirs.push_back(new TestDirectoryTree(_path / "SubDirC", nLevels - 1));
            }

            std::vector<XXH64_hash_t> hashes;
            for (auto f : _files) hashes.push_back(XXH64_string(f.string()));
            for (auto d : _subDirs) hashes.push_back(XXH64_string(d->path().string()));
            _hash = XXH64(hashes.data(), sizeof(XXH64_hash_t) * hashes.size(), 0);
        }

        ~TestDirectoryTree() {
            for (auto f : _files) std::filesystem::remove(f);
            for (auto d : _subDirs) delete d;
            std::filesystem::remove_all(_path);
        }

        std::filesystem::path const& path() const { return _path; }
        unsigned int nLevels() const { return _nLevels; }

        std::vector<std::filesystem::path> const& getFiles() const { return _files; }
        std::vector<TestDirectoryTree*> const& getSubDirs() const { return _subDirs; }

        XXH64_hash_t getHash() const { return _hash; }

    private:
        std::filesystem::path _path;
        unsigned int _nLevels;
        std::vector<std::filesystem::path> _files;
        std::vector<TestDirectoryTree*> _subDirs;
        XXH64_hash_t _hash;
    };
}

namespace
{
    using namespace YAM;

    //TODO: once DirectoryNode is made to construct the entire tree
    //must be adapted to verify the entire tree.
    void verify(TestDirectoryTree* expected, DirectoryNode* actual) {

        EXPECT_EQ(Node::State::Dirty, actual->state());
        bool completed = YAMTest::executeNode(actual);

        EXPECT_TRUE(completed);
        EXPECT_EQ(Node::State::Ok, actual->state());
        EXPECT_EQ(expected->getHash(), actual->getHash());

        std::vector<FileNode*> fileNodes;
        actual->getFiles(fileNodes);
        EXPECT_EQ(expected->getFiles().size(), fileNodes.size());
        for (std::size_t i = 0; i < fileNodes.size(); ++i) {
            EXPECT_EQ(expected->getFiles()[i], fileNodes[i]->name());
        }

        std::vector<DirectoryNode*> subDirNodes;
        actual->getSubDirs(subDirNodes);
        EXPECT_EQ(expected->getSubDirs().size(), subDirNodes.size());
        for (std::size_t i = 0; i < subDirNodes.size(); ++i) {
            EXPECT_EQ(expected->getSubDirs()[i]->path(), subDirNodes[i]->name());
        }

        std::map<std::filesystem::path, Node*> const& cNodes = actual->getContent();
        EXPECT_EQ(expected->getFiles().size() + expected->getSubDirs().size(), cNodes.size());
        for (auto f : fileNodes) EXPECT_TRUE(cNodes.contains(f->name()));
        for (auto d : subDirNodes) EXPECT_TRUE(cNodes.contains(d->name()));

        for (std::size_t i = 0; i < subDirNodes.size(); ++i) {
            verify(expected->getSubDirs()[i], subDirNodes[i]);
        }
    }
    TEST(DirectoryNode, dirWithZeroSubDirs) {
        std::filesystem::path rootDir(std::filesystem::temp_directory_path() / std::tmpnam(nullptr));
        TestDirectoryTree testTree(rootDir, 0);

        ExecutionContext context;
        DirectoryNode dirNode(&context, rootDir);

        verify(&testTree, &dirNode);
    }
    TEST(DirectoryNode, dirWithThreeSubDirs) {
        std::filesystem::path rootDir(std::filesystem::temp_directory_path() / std::tmpnam(nullptr));
        TestDirectoryTree testTree(rootDir, 3);

        ExecutionContext context;
        DirectoryNode dirNode(&context, rootDir);

        verify(&testTree, &dirNode);
    }
}
