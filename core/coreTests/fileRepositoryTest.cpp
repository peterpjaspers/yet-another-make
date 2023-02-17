#include "../FileRepository.h"

#include "gtest/gtest.h"

namespace
{
	using namespace YAM;

	const std::filesystem::path repoDir(R"(C:\aap\noot\mies)");
	const std::string repoName("testRepo");
	FileRepository repo(repoName, repoDir);

	TEST(FileRepository, construct) {
		EXPECT_EQ(repoName, repo.name());
		EXPECT_EQ(repoDir, repo.directory());
	}

	TEST(FileRepository, contains) {
		EXPECT_TRUE(repo.contains(R"(C:\aap\noot\mies\file.cpp)"));
		EXPECT_FALSE(repo.contains(R"(C:\aap\noot\file.cpp)"));
		EXPECT_FALSE(repo.contains(R"(\aap\noot\mies\file.cpp)"));
		EXPECT_FALSE(repo.contains(R"(aap\noot\mies\file.cpp)"));
	}

	TEST(FileRepository, relativePath) {
		EXPECT_EQ(std::filesystem::path(R"(file.cpp)"), repo.relativePathOf(R"(C:\aap\noot\mies\file.cpp)"));
		EXPECT_EQ(std::filesystem::path(), repo.relativePathOf(R"(C:\aap\noot\file.cpp)"));
		EXPECT_EQ(std::filesystem::path(), repo.relativePathOf(R"(\aap\noot\file.cpp)"));
		EXPECT_EQ(std::filesystem::path(), repo.relativePathOf(R"(aap\noot\mies\file.cpp)"));
	}

	TEST(FileRepository, symbolicPath) {
		EXPECT_EQ(std::filesystem::path(R"(testRepo\file.cpp)"), repo.symbolicPathOf(R"(C:\aap\noot\mies\file.cpp)"));
		EXPECT_EQ(std::filesystem::path(), repo.symbolicPathOf(R"(C:\aap\noot\file.cpp)"));
		EXPECT_EQ(std::filesystem::path(), repo.symbolicPathOf(R"(\aap\noot\file.cpp)"));
		EXPECT_EQ(std::filesystem::path(), repo.symbolicPathOf(R"(aap\noot\mies\file.cpp)"));
	}
}