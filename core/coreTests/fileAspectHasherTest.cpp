#include "../FileAspectHasher.h"
#include "../Delegates.h"
#include "../../xxHash/xxhash.h"

#include "gtest/gtest.h"
#include <cstdio>

namespace
{
	using namespace YAM;

	std::string testString("/*dit is een fileaspect hasher test string*/");
	std::filesystem::path testPath(std::filesystem::temp_directory_path() / "fileHasherTest.cpp");

	bool createTestFile(std::string const& content) {
		FILE* fp = std::fopen(testPath.string().c_str(), "w");
		if (!fp) return false;

		auto nWritten = std::fwrite(content.c_str(), sizeof(char), content.length(), fp);
		if (nWritten != content.length()) return false;

		std::fclose(fp);
		return true;
	}

	XXH64_hash_t hashString(std::string const& content) {
		return XXH64(content.c_str(), content.length(), 0);
	}

	Delegate<XXH64_hash_t, std::filesystem::path const&> hashEntireFileDelegate =
		Delegate<XXH64_hash_t, std::filesystem::path const&>::CreateLambda(
			[](std::filesystem::path const& fn) {
				return XXH64_file(fn.string().c_str()); 
			});


	TEST(FileAspectHasher, constructAndHash) {

		FileAspect aspect("cpp-code", RegexSet({ "\\.cpp$" , "\\.c$", "\\.h$" }));;
		FileAspectHasher hasher(aspect, hashEntireFileDelegate);

		EXPECT_EQ("cpp-code", hasher.aspect().name());
		EXPECT_TRUE(hasher.aspect().matches(testPath));
		EXPECT_TRUE(createTestFile(testString));
		XXH64_hash_t expectedHash = hashString(testString);
		EXPECT_EQ(expectedHash, hasher.hash(testPath));
		std::filesystem::remove(testPath);
	}
}