#pragma once

/* This way of debugging memory leaks in unit-tests (see coreTests\main_gtest.cpp)
 * was not very usefull because the debug output did not show where the leaked
 * memory buffers were allocated.
 *
 *This header file is forced included in the 'Forced include file' option of the compiler.
 *See compiler options in the core and coreTests project.

#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

*/

