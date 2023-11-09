#include "../FileNode.h"
#include "../ExecutionContext.h"
#include "../SourceFileRepository.h"
#include "../FileSystem.h"

#include "gtest/gtest.h"
#include <memory>
#include <vector>
#include <unordered_set>

namespace
{
    using namespace YAM;

    class RepoProps {
    public:
        RepoProps(std::string const& rname, std::filesystem::path const& rpath)
            : name(rname)
            , dir(rpath)
        {
            std::filesystem::remove_all(rname);
            std::filesystem::create_directories(rpath);
        }
        ~RepoProps() {
            std::filesystem::remove_all(dir);
        }
        std::string name;
        std::filesystem::path dir;
    };

    class ContextSetup
    {
    public:
        std::vector<std::shared_ptr<Node>> nodes;
        RepoProps repo1;
        RepoProps repo2;
        RepoProps repo3;
        std::vector<std::shared_ptr<SourceFileRepository>> repos;
        ExecutionContext context;

        ContextSetup()
            : repo1("repo1", FileSystem::createUniqueDirectory())
            , repo2("repo2", FileSystem::createUniqueDirectory())
            , repo3("repo3", FileSystem::createUniqueDirectory())
        {
            nodes.push_back(std::make_shared<FileNode>(&context, "n1"));
            nodes.push_back(std::make_shared<FileNode>(&context, "n2"));
            nodes.push_back(std::make_shared<FileNode>(&context, "n3"));
            repos.push_back(std::make_shared<SourceFileRepository>(repo1.name, repo1.dir, &context));
            repos.push_back(std::make_shared<SourceFileRepository>(repo2.name, repo2.dir, &context));
            repos.push_back(std::make_shared<SourceFileRepository>(repo3.name, repo3.dir, &context));
            for (auto n : nodes) context.nodes().add(n);
            for (auto r : repos) context.addRepository(r);
        }
    };

    TEST(ExecutionContext, getBuildState) {
        ContextSetup setup;

        std::unordered_set<std::shared_ptr<IPersistable>> buildState;
        setup.context.getBuildState(buildState);
        EXPECT_EQ(
            setup.nodes.size() // 3 file nodes
            + setup.repos.size() // 3 repo root dir nodes
            + setup.repos.size() * 3 // per repo dir .ignore, .yamignore, .gitignore
            + setup.repos.size(), // 3 repos
            buildState.size());
        for (auto n : setup.nodes) EXPECT_TRUE(buildState.contains(n));
        for (auto r : setup.repos) EXPECT_TRUE(buildState.contains(r));
    }

    TEST(ExecutionContext, clearBuildState) {
        ContextSetup setup;

        setup.context.clearBuildState();
        std::unordered_set<std::shared_ptr<IPersistable>> buildState;
        setup.context.getBuildState(buildState);
        EXPECT_EQ(0, buildState.size());
    }

    TEST(ExecutionContext, computeStorageNeedInsertAll) {
        ContextSetup setup;

        std::unordered_set<std::shared_ptr<IPersistable>> buildState;
        setup.context.getBuildState(buildState);
        std::unordered_set<std::shared_ptr<IPersistable>> storedState;

        std::unordered_set<std::shared_ptr<IPersistable>> toInsert;
        std::unordered_set<std::shared_ptr<IPersistable>> toReplace;
        std::unordered_set<std::shared_ptr<IPersistable>> toRemove;
        setup.context.computeStorageNeed(buildState, storedState, toInsert, toReplace, toRemove);

        EXPECT_EQ(
            setup.nodes.size() // 3 file nodes
            + setup.repos.size() // 3 repo root dir nodes
            + setup.repos.size() * 3 // 9, per repo dir .ignore, .yamignore, .gitignore
            + setup.repos.size(), // 3 repos
            toInsert.size());
        for (auto n : setup.nodes) EXPECT_TRUE(toInsert.contains(n));
        for (auto r : setup.repos) EXPECT_TRUE(toInsert.contains(r));
        EXPECT_EQ(0, toReplace.size());
        EXPECT_EQ(0, toRemove.size());
    }

    TEST(ExecutionContext, computeStorageNeedReplaceAll) {
        ContextSetup setup;

        std::unordered_set<std::shared_ptr<IPersistable>> buildState;
        setup.context.getBuildState(buildState);
        std::unordered_set<std::shared_ptr<IPersistable>> storedState;
        setup.context.getBuildState(storedState);

        std::unordered_set<std::shared_ptr<IPersistable>> toInsert;
        std::unordered_set<std::shared_ptr<IPersistable>> toReplace;
        std::unordered_set<std::shared_ptr<IPersistable>> toRemove;
        setup.context.computeStorageNeed(buildState, storedState, toInsert, toReplace, toRemove);

        // Note that all objects in build state are in modified() state.
        EXPECT_EQ(0, toInsert.size());
        EXPECT_EQ(
            setup.nodes.size() // 3 file nodes
            + setup.repos.size() // 3 repo root dir nodes
            + setup.repos.size() * 3 // 9, per repo dir .ignore, .yamignore, .gitignore
            + setup.repos.size(), // 3 repos
            toReplace.size());
        for (auto n : setup.nodes) EXPECT_TRUE(toReplace.contains(n));
        for (auto r : setup.repos) EXPECT_TRUE(toReplace.contains(r));
        EXPECT_EQ(0, toRemove.size());
    }

    TEST(ExecutionContext, computeStorageNeedEmpty) {
        ContextSetup setup;

        std::unordered_set<std::shared_ptr<IPersistable>> buildState;
        setup.context.getBuildState(buildState);
        for (auto obj : buildState) obj->modified(false);
        std::unordered_set<std::shared_ptr<IPersistable>> storedState;
        setup.context.getBuildState(storedState);

        std::unordered_set<std::shared_ptr<IPersistable>> toInsert;
        std::unordered_set<std::shared_ptr<IPersistable>> toReplace;
        std::unordered_set<std::shared_ptr<IPersistable>> toRemove;
        setup.context.computeStorageNeed(buildState, storedState, toInsert, toReplace, toRemove);

        EXPECT_EQ(0, toInsert.size());
        EXPECT_EQ(0, toReplace.size());
        EXPECT_EQ(0, toRemove.size());
    }

    TEST(ExecutionContext, computeStorageNeedRemoveAll) {
        ContextSetup setup;

        std::unordered_set<std::shared_ptr<IPersistable>> buildState;
        std::unordered_set<std::shared_ptr<IPersistable>> storedState;
        setup.context.getBuildState(storedState);
        for (auto obj : storedState) obj->modified(false);

        std::unordered_set<std::shared_ptr<IPersistable>> toInsert;
        std::unordered_set<std::shared_ptr<IPersistable>> toReplace;
        std::unordered_set<std::shared_ptr<IPersistable>> toRemove;
        setup.context.computeStorageNeed(buildState, storedState, toInsert, toReplace, toRemove);

        EXPECT_EQ(0, toInsert.size());
        EXPECT_EQ(0, toReplace.size());
        EXPECT_EQ(
            setup.nodes.size() // 3 file nodes
            + setup.repos.size() // 3 repo root dir nodes
            + setup.repos.size() * 3 // 9, per repo dir .ignore, .yamignore, .gitignore
            + setup.repos.size(), // 3 repos
            toRemove.size());
        for (auto n : setup.nodes) EXPECT_TRUE(toRemove.contains(n));
        for (auto r : setup.repos) EXPECT_TRUE(toRemove.contains(r));
    }
}
