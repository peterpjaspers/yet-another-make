#ifndef LANG_SYMBOL_TABLE_H
#define LANG_SYMBOL_TABLE_H

#include "Types.h"
#include "LogFile.h"

#include <string>
#include <filesystem>
#include <iostream>
#include <vector>

namespace Language {

    // Look-up a symbol with the given name in the current scope.
    // The local argument controls scope locality, false to look-up in full hierarchy.
    // Returns the Descriptor value of the symbol if it exists, otherwise return NullDescriptor.
    Descriptor lookUpSymbol( const std::string& name, const std::vector<std::string>& scope, bool local = false );
    // Define a symbol with the given name in the current local scope.
    void defineSymbol( const std::string& name, const std::vector<std::string>& scope, const Descriptor value );

    void printSymbolTable( LogFile& stream );
    void printSymbolTable( const std::filesystem::path file );

}

#endif // LANG_SYMBOL_TABLE_H
