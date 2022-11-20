#include "../InputFileAspects.h"

#include "gtest/gtest.h"

namespace
{
	using namespace YAM;

	TEST(InputFileAspects, construct) {
		FileAspectSet aspects;
		FileAspect codeAspect("cpp-code", RegexSet({ "\\.cpp$",  "\\.c$" "\\.h$" }));
		aspects.add(codeAspect);

		// An object file is compiled from a .cpp/.c file and from the .h files included
		// by that .cpp/.c file.  Changes in comment and empty lines in those files do not 
		// affect the compilation result. YAM makes it possible to ignore such changes:
		//   - by associating the cpp-code aspect with a FileAspectHasher that excludes
		//     comments and empty lines from the hash computation (not part of this test)
		//   - by associating the inputfile aspects of .obj files with the cpp-code aspect
		//     (as shown in this test).
		//   - by computing the execution hash of the compilation command from the cpp-code
		//     aspect hashes of the compiled input files.
		InputFileAspects inputAspects("\\.obj", aspects);
		EXPECT_EQ("\\.obj", inputAspects.outputFileNamePattern());
		EXPECT_EQ(codeAspect.name(), inputAspects.inputAspects().aspects()[0].name());
	}

	TEST(InputFileAspects, matches) {
		FileAspectSet aspects;
		FileAspect codeAspect("cpp-code", RegexSet({ "\\.cpp$",  "\\.c$" "\\.h$" }));
		aspects.add(codeAspect);

		InputFileAspects inputAspects("\\.obj", aspects);
		EXPECT_TRUE(inputAspects.matches("source.obj"));
		EXPECT_FALSE(inputAspects.matches("source.dll"));
	}
}