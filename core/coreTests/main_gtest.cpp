#include "gtest/gtest.h"
#include <cstdio>

GTEST_API_ int main(int argc, char** argv) {
    printf("Running main() from %s\n", __FILE__);
    testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();

// This way of debugging memory leaks in unit-tests (see coreTests\main_gtest.cpp)
// was not very usefull because the debug output did not show where the leaked
// memory buffers were allocated. 
// _CRTDBG_MAP_ALLOC is defined in core\dbgMemLeaks.h. This header file is forced
// include in the 'Forced include file' option of the compiler. See compiler options
// in the core and coreTests project.
#ifdef _CRTDBG_MAP_ALLOC
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
    _CrtDumpMemoryLeaks();
#endif

    return result;
}