#include "SymbolTable.h"
#include "Monitor.h"
#include "Descriptor.h"

using namespace std;
using namespace std::filesystem;

namespace Language {

    std::string qualifySymbolName( const ThreadContext& cctx, const std::string& name, int depth ) {
        string qualifiedName( "" );
        if (depth == 0) depth = cctx.scope.size();
        for ( int i = 0; i < depth; ++i ) qualifiedName += cctx.scope[ i ] + ":";
        qualifiedName += name;
        return qualifiedName;
    }
    Descriptor lookUpSymbol( ThreadContext& ctx, const std::string& name, bool local ) {
        int depth( ctx.scope.size() );
        if (local) {
            auto found( ctx.symbols.find( qualifySymbolName( name, depth ) ) );
            if (found != ctx.symbols.end()) return found->second;
        } else {
            while (0 <= depth) {
                auto found( ctx.symbols.find( qualifySymbolName( name, depth ) ) );
                if (found != ctx.symbols.end()) return found->second;
                depth -= 1;
            }
        }
        return NullDescriptor();
    }
    void defineSymbol( ThreadContext& ctx, const std::string& name, const Descriptor value ) {
        auto qualifiedName( qualifySymbolName( name, ctx.scope.size() ) );
        ctx.symbols.insert( { qualifiedName, value } );
    }

    void printSymbolTable( const ThreadContext& cctx, LogFile& stream ) {
        for ( auto entry : cctx.symbols ) {
            ostream& output = stream() << setw( 32 ) << entry.first << " -> ";
            auto descriptor( entry.second );
            auto typeWord( type( descriptor ) );
            auto value( word( descriptor ) );
            if (typeWord == LocalVariableTypeWord) output << setw( 11 ) << "Local[ " << setw( 4 ) << value << " ]" << record<char>;
            else if (typeWord == GlobalVariableTypeWord) output << setw( 11 ) << "Global[ " << setw( 4 ) << value << " ]" << record<char>;
            else if (typeWord == ArgumentVariableTypeWord) output << setw( 11 ) << "Argument[ " << setw( 4 ) << value << " ]" << record<char>;
            else if (typeWord == ProcedureTypeWord) output << setw( 11 ) << "Procedure[ " << setw( 4 ) << value << " ]" << record<char>;
            else if (typeWord == IntrinsicTypeWord) output << setw( 11 ) << "Intrinsic[ " << setw( 4 ) << value << " ]" << record<char>;
            else output << setw( 11 ) << "Undefined[ " << setw( 4 ) << value << " ]" << record<char>;
        }
    }
    void printSymbolTable(  const ThreadContext& cctx, const path file ) {
        LogFile stream( file, false, false );
        return printSymbolTable( cctx, stream );
    }

} // namespace Language