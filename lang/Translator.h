#ifndef LANG_TRANSLATOR_H
#define LANG_TRANSLATOR_H

#include "Types.h"
#include "ThreadContext.h"

#include <string>
#include <filesystem>
#include <vector>

namespace Language {

    // Translate program source as string or file into byte-code in program memory.
    // Returns start address of translated program.
    // Generates null program when translation errors are detected.
    Address translate( ThreadContext& ctx, const std::string& program, Word base = 0 );
    Address translate( ThreadContext& ctx, const std::filesystem::path& program, Word base = 0 );
    inline Address translate( const std::string& program, Word base = 0 ) { return translate( context(), program, base ); }
    inline Address translate( const std::filesystem::path& program, Word base = 0 ) { return translate( context(), program, base ); }

}

#endif // LANG_TRANSLATOR_H
