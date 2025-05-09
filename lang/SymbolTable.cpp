#include "SymbolTable.h"
#include "Monitor.h"
#include "Descriptor.h"

#include <map>

using namespace std;
using namespace std::filesystem;

namespace Language {

    namespace { map<string,Descriptor> symbolTable; }

    string scopedName( const std::string& name, const vector<string>& scope, int depth ) {
        string qualifiedName( "" );
        for ( int i = 0; i < depth; ++i ) qualifiedName += scope[ i ] + ":";
        qualifiedName += name;
        return qualifiedName;
    }
    Descriptor lookUpSymbol( const std::string& name, const vector<string>& scope, bool local ) {
        int depth( scope.size() );
        if (local) {
            auto found( symbolTable.find( scopedName( name, scope, depth ) ) );
            if (found != symbolTable.end()) return found->second;
        } else {
            while (0 <= depth) {
                auto found( symbolTable.find( scopedName( name, scope, depth ) ) );
                if (found != symbolTable.end()) return found->second;
                depth -= 1;
            }
        }
        return NullDescriptor();
    }
    void defineSymbol( const std::string& name, const vector<string>& scope, const Descriptor value ) {
        auto qualifiedName( scopedName( name, scope, scope.size() ) );
        symbolTable.insert( { qualifiedName, value } );
    }

    void printSymbolTable( LogFile& stream ) {
        for ( auto entry : symbolTable ) {
            ostream& output = stream() << setw( 32 ) << entry.first << " : ";
            auto descriptor( entry.second );
            auto typeWord( type( descriptor ) );
            auto value( word( descriptor ) );
            if (typeWord == LocalVariableTypeWord) output << setw( 11 ) << "Local[ " << setw( 4 ) << value << " ]" << record<char>;
            else if (typeWord == GlobalVariableTypeWord) output << setw( 11 ) << "Global[ " << setw( 4 ) << value << " ]" << record<char>;
            else if (typeWord == ArgumentVariableTypeWord) output << setw( 11 ) << "Argument[ " << setw( 4 ) << value << " ]" << record<char>;
            else if (typeWord == ProcedureTypeWord) output << setw( 11 ) << "Procedure[ " << setw( 4 ) << value << " ]" << record<char>;
            else output << setw( 11 ) << "Undefined[ " << setw( 4 ) << value << " ]" << record<char>;
        }
    }
    void printSymbolTable( const path file ) {
        LogFile stream( file, false, false );
        return printSymbolTable( stream );
    }

} // namespace Language