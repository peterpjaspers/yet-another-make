#include "../FileNode.h"
#include "../ExecutionContext.h"
#include "../FileRepositoryNode.h"
#include "../FileSystem.h"
#include "../RepositoriesNode.h"

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
        std::vector<std::shared_ptr<FileRepositoryNode>> repos;
        ExecutionContext context;

        ContextSetup()
            : repo1("repo1", FileSystem::createUniqueDirectory())
            , repo2("repo2", FileSystem::createUniqueDirectory())
            , repo3("repo3", FileSystem::createUniqueDirectory())
        {
            nodes.push_back(std::make_shared<FileNode>(&context, "n1"));
            nodes.push_back(std::make_shared<FileNode>(&context, "n2"));
            nodes.push_back(std::make_shared<FileNode>(&context, "n3"));           
            for (auto n : nodes) context.nodes().add(n);

            auto homeRepo = std::make_shared<FileRepositoryNode>(&context, repo1.name, repo1.dir, false);
            auto repositories = std::make_shared<RepositoriesNode>(&context, homeRepo);
            context.repositoriesNode(repositories);

            repos.push_back(homeRepo);
            repos.push_back(std::make_shared<FileRepositoryNode>(&context, repo2.name, repo2.dir, false));
            repos.push_back(std::make_shared<FileRepositoryNode>(&context, repo3.name, repo3.dir, false));

            for (auto n : repos) repositories->addRepository(n);
        }
    };

    TEST(ExecutionContext, getBuildState) {
        ContextSetup setup;

        std::unordered_set<std::shared_ptr<IPersistable>> buildState;
        setup.context.getBuildState(buildState);
        EXPECT_EQ(
            setup.nodes.size() // 3 file nodes
            + 2 // RepositoriesNode + its config file
            + setup.repos.size() // 3 FileRepositoryNode
            + setup.repos.size() // 3 repo root dir nodes
            + setup.repos.size() * 3 // per repo dir .ignore, .yamignore, .gitignore
            + setup.repos.size() * 2, // per repo FileExecSpecs + its configfile
            buildState.size());
        for (auto n : setup.nodes) EXPECT_TRUE(buildState.contains(n));
        EXPECT_TRUE(buildState.contains(setup.context.repositoriesNode()));
    }

    TEST(ExecutionContext, clearBuildState) {
        ContextSetup setup;

        setup.context.clearBuildState();
        std::unordered_set<std::shared_ptr<IPersistable>> buildState;
        setup.context.getBuildState(buildState);
        EXPECT_EQ(0, buildState.size());
    }
}
