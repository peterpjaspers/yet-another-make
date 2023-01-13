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

            _files.push_back(_path / "File1");
            _files.push_back(_path / "File2");
            _files.push_back(_path / "File3");
            for (auto f : _files) std::ofstream{ f };

            if (_nLevels > 0) {
                _subDirs.push_back(new TestDirectoryTree(_path / "SubDir1", nLevels - 1));
                _subDirs.push_back(new TestDirectoryTree(_path / "SubDir2", nLevels - 1));
                _subDirs.push_back(new TestDirectoryTree(_path / "SubDir3", nLevels - 1));
            }

            updateHash();
        }

        ~TestDirectoryTree() {
            for (auto f : _files) std::filesystem::remove(f);
            for (auto d : _subDirs) delete d;
            std::filesystem::remove_all(_path);
        }

        // add a file at specified level in tree
        void addFile() {
            char buf[32];
            const std::string nextIndex(_itoa((int)(_files.size() + 1), buf, 10));
            const std::string nextName("File" + nextIndex);
            const std::filesystem::path nextPath(_path / nextName);
            _files.push_back(nextPath);
            std::ofstream{ nextPath };
            updateHash();
        }

        void addDirectory() { 
            if (_nLevels == 0) _nLevels = 1;
            char buf[32];
            const std::string nextIndex(_itoa((int)(_subDirs.size() + 1), buf, 10));
            const std::string nextName("SubDir" + nextIndex);
            const std::filesystem::path nextPath(_path / nextName);
            _subDirs.push_back(new TestDirectoryTree(nextPath, _nLevels - 1));
            std::ofstream{ nextPath };
            updateHash();
        }

        std::filesystem::path const& path() const { return _path; }
        unsigned int nLevels() const { return _nLevels; }

        std::vector<std::filesystem::path> const& getFiles() const { return _files; }
        std::vector<TestDirectoryTree*> const& getSubDirs() const { return _subDirs; }

        XXH64_hash_t getHash() const { return _hash; }

    private:
        void updateHash() {
            std::vector<XXH64_hash_t> hashes;
            for (auto f : _files) hashes.push_back(XXH64_string(f.string()));
            for (auto d : _subDirs) hashes.push_back(XXH64_string(d->path().string()));
            _hash = XXH64(hashes.data(), sizeof(XXH64_hash_t) * hashes.size(), 0);
        }
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

    void verify(TestDirectoryTree* expected, DirectoryNode* actual) {

        EXPECT_EQ(Node::State::Ok, actual->state());
        EXPECT_EQ(expected->getHash(), actual->getHash());

        std::vector<std::shared_ptr<FileNode>> fileNodes;
        actual->getFiles(fileNodes);
        EXPECT_EQ(expected->getFiles().size(), fileNodes.size());
        for (std::size_t i = 0; i < fileNodes.size(); ++i) {
            EXPECT_EQ(expected->getFiles()[i], fileNodes[i]->name());
        }

        std::vector<std::shared_ptr<DirectoryNode>> subDirNodes;
        actual->getSubDirs(subDirNodes);
        EXPECT_EQ(expected->getSubDirs().size(), subDirNodes.size());
        for (std::size_t i = 0; i < subDirNodes.size(); ++i) {
            EXPECT_EQ(expected->getSubDirs()[i]->path(), subDirNodes[i]->name());
        }

        std::map<std::filesystem::path, std::shared_ptr<Node>> const& cNodes = actual->getContent();
        EXPECT_EQ(expected->getFiles().size() + expected->getSubDirs().size(), cNodes.size());
        for (auto f : fileNodes) EXPECT_TRUE(cNodes.contains(f->name()));
        for (auto d : subDirNodes) EXPECT_TRUE(cNodes.contains(d->name()));

        for (std::size_t i = 0; i < subDirNodes.size(); ++i) {
            verify(expected->getSubDirs()[i], subDirNodes[i].get());
        }
    }
    TEST(DirectoryNode, construct_twoDeepDirectoryTree) {
        std::string tmpDir(std::tmpnam(nullptr));
        std::filesystem::path rootDir(std::string(tmpDir + "_dirNodeTest"));
        TestDirectoryTree testTree(rootDir, 2);

        // Create the directory node tree that reflects testTree
        ExecutionContext context;
        DirectoryNode dirNode(&context, rootDir);
        EXPECT_EQ(Node::State::Dirty, dirNode.state());

        bool completed = YAMTest::executeNode(&dirNode);
        EXPECT_TRUE(completed);
        verify(&testTree, &dirNode);
    }

    TEST(DirectoryNode, update_threeDeepDirectoryTree) {
        std::string tmpDir(std::tmpnam(nullptr));
        std::filesystem::path rootDir(std::string(tmpDir + "_dirNodeTest"));
        TestDirectoryTree testTree(rootDir, 3);

        // Create the directory node tree that reflects testTree
        ExecutionContext context;
        DirectoryNode dirNode(&context, rootDir);
        bool completed = YAMTest::executeNode(&dirNode);
        EXPECT_TRUE(completed);

        // Update file system
        TestDirectoryTree* testTree_S1 = testTree.getSubDirs()[1];
        TestDirectoryTree* testTree_S1_S2 = testTree_S1->getSubDirs()[2];
        testTree.addFile();
        testTree_S1->addFile();
        testTree_S1_S2->addDirectory();
        testTree_S1_S2->addFile();

        // Find nodes affected by file system changes...
        std::vector<std::shared_ptr<DirectoryNode>> subDirNodes;
        dirNode.getSubDirs(subDirNodes);
        std::shared_ptr<DirectoryNode> dirNode_S1 = subDirNodes[1];
        subDirNodes.clear();
        dirNode_S1->getSubDirs(subDirNodes);
        std::shared_ptr<DirectoryNode> dirNode_S1_S2 = subDirNodes[2];

        // ...and mark these nodes dirty
        dirNode.setState(Node::State::Dirty);
        dirNode_S1->setState(Node::State::Dirty);
        dirNode_S1_S2->setState(Node::State::Dirty);

        // re-execute directory node tree to sync with changed testTree
        completed = YAMTest::executeNode(&dirNode);
        EXPECT_TRUE(completed);

        verify(&testTree, &dirNode);
    }
}
