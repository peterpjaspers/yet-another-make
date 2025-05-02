#ifndef LANG_SYMBOL_TABLE_H
#define LANG_SYMBOL_TABLE_H

#include "Types.h"
#include "LogFile.h"

#include <string>
#include <filesystem>
#include <iostream>

namespace Language {

    enum SymbolType {
        None,
        LocalVariable,
        GlobalVariable,
        ArgumentVariable,
        Procedure
    };

    SymbolType symbolType( const std::string& name );
    void createLocalVariable( const std::string& name, const Word offset );
    Word localVariableOffset( const std::string& name );
    void createArgumentVariable( const std::string& name, const Address address );
    Address argumentVariableAddress( const std::string& name );
    void createGlobalVariable( const std::string& name, const Address address );
    Address globalVariableAddress( const std::string& name );
    void createProcedure( const std::string& name, const Address address );
    Address procedureAddress( const std::string& name );

    void printSymbolTable( LogFile<char>& stream );
    void printSymbolTable( const std::filesystem::path file );

}

#endif // LANG_SYMBOL_TABLE_H
