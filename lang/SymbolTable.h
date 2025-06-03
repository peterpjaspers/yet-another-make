#ifndef LANG_SYMBOL_TABLE_H
#define LANG_SYMBOL_TABLE_H

#include "Types.h"
#include "LogFile.h"
#include "ThreadContext.h"

#include <string>
#include <filesystem>

namespace Language {

    // Symbols are defined within a scope hierarchy. Each statement block defines a local scope.
    // Procedures and names spaces are named scopes, statemente blocks are anonymous scopes that
    // are are indexed according to their order. A fully qualified symbol is qualified by the
    // entire scope hierarchy.

    // Enter a named scope within the current scope.
    inline void enterNamedScope( ThreadContext& ctx, const std::string& name ) { ctx.scope.push_back( name ); }
    inline void enterNamedScope( const std::string& name ) { enterNamedScope( context(), name ); }
    // Enter an anonymous scope within the current scope.
    inline void enterAnonymousScope( ThreadContext& ctx, const int index ) { ctx.scope.push_back( std::to_string( index ) ); }
    inline void enterAnonymousScope( const int index ) { enterAnonymousScope( context(), index ); }
    // Exit the current scope.
    inline void exitScope( ThreadContext& ctx ) { ctx.scope.pop_back(); }
    inline void exitScope() { exitScope( context() ); }

    // Qualify a name with the current scope.
    // The optional depth argument defines the scope nesting level to which the name is to be qualified.
    std::string qualifySymbolName( const ThreadContext& cctx, const std::string& name, int depth = 0 );
    inline std::string qualifySymbolName( const std::string& name, int depth = 0 ) { return qualifySymbolName( ccontext(), name, depth ); }

    // Look-up a symbol with the given name in the current scope.
    // The local argument controls scope locality, false to look-up in full hierarchy.
    // Returns the Descriptor value of the symbol if it exists, otherwise return NullDescriptor.
    Descriptor lookUpSymbol( ThreadContext& ctx, const std::string& name, bool local = false );
    inline Descriptor lookUpSymbol( const std::string& name, bool local = false ) { lookUpSymbol( context(), name, local ); }
    // Define a symbol with the given name in the current local scope.
    void defineSymbol( ThreadContext& ctx, const std::string& name, const Descriptor value );
    inline void defineSymbol( const std::string& name, const Descriptor value ) { defineSymbol( context(), name, value ); }

    void printSymbolTable( const ThreadContext& cctx, LogFile& stream );
    void printSymbolTable( const ThreadContext& cctx, const std::filesystem::path file );
    inline void printSymbolTable( LogFile& stream ) { printSymbolTable( ccontext(), stream ); }
    inline void printSymbolTable( const std::filesystem::path file ) { printSymbolTable( ccontext(), file ); }

}

#endif // LANG_SYMBOL_TABLE_H
