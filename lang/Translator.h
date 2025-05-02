#ifndef LANG_TRANSLATOR_H
#define LANG_TRANSLATOR_H

#include "Types.h"

#include <string>
#include <filesystem>
#include <fstream>

namespace Language {

    // Translate program source as string or file into byte-code in program memory.
    // Returns start address of translated program.
    // Generates null program when translation errors are detected.
    Address translate( std::string program );
    Address translate( std::filesystem::path program );

}

#endif // LANG_TRANSLATOR_H
