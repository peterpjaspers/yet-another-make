#include "SymbolTable.h"

#include <map>

using namespace std;
using namespace std::filesystem;

namespace Language {

    namespace {

        struct SymbolEntry {
            SymbolType type;
            union SymbolValue {
                Address address;    // Address in program memeory of procedure entry-point
                Address heap;       // Address in heap memory for global variable
                Word offset;        // Offset from frame-pointer for local variable or argument
            } value;
        };

        map<string,SymbolEntry> symbolTable;
    
    }

    void createLocalVariable( const std::string& name, const Word offset ) {
        SymbolEntry entry;
        entry.type = LocalVariable;
        entry.value.offset = offset;
        symbolTable.insert( { name, entry } );
    }
    Word localVariableOffset( const std::string& name ) {
        auto entry( symbolTable.at( name ) );
        return entry.value.offset;
    }
    void createArgumentVariable( const std::string& name, const Address address ) {
        SymbolEntry entry;
        entry.type = ArgumentVariable;
        entry.value.address = address;
        symbolTable.insert( { name, entry } );
    }
    Address argumentVariableAddress( const std::string& name ) {
        auto entry( symbolTable.at( name ) );
        return entry.value.heap;
    }
    void createGlobalVariable( const std::string& name, const Address address ) {
        SymbolEntry entry;
        entry.type = GlobalVariable;
        entry.value.address = address;
        symbolTable.insert( { name, entry } );
    }
    Address globalVariableAddress( const std::string& name ) {
        auto entry( symbolTable.at( name ) );
        return entry.value.heap;
    }
    void createProcedure( const std::string& name, const Address address ) {
        SymbolEntry entry;
        entry.type = Procedure;
        entry.value.address = address;
        symbolTable.insert( { name, entry } );

    }
    Address procedureAddress( const std::string& name ) {
        auto entry( symbolTable.at( name ) );
        return entry.value.address;
    }
    SymbolType symbolType( const std::string& name ) {
        auto entry( symbolTable.find( name ) );
        if (entry == symbolTable.end()) return None;
        return entry->second.type;
    }

    void printSymbolTable( LogFile<char>& stream ) {
        for ( auto entry : symbolTable ) {
            ostream& output = stream() << setw( 32 ) << entry.first << " : ";
            switch (entry.second.type) {
                case None : output << setw( 11 ) << "None" << record<char>; break;
                case LocalVariable : output << setw( 11 ) << "Local[ " << setw( 4 ) << entry.second.value.offset << " ]" << record<char>; break;
                case GlobalVariable : output << setw( 11 ) << "Global[ " << setw( 4 ) << entry.second.value.heap << " ]" << record<char>; break;
                case ArgumentVariable : output << setw( 11 ) << "Argument[ " << setw( 4 ) << entry.second.value.heap << " ]" << record<char>; break;
                case Procedure : output << setw( 11 ) << "Procedure[ " << setw( 4 ) << entry.second.value.address << " ]" << record<char>; break;
            } 
        }
    }
    void printSymbolTable( const path file ) {
        LogFile<char> stream( file, false, false );
        return printSymbolTable( stream );
    }

} // namespace Language