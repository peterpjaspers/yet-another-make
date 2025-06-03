#ifndef LANG_THREAD_CONTEXT_H
#define LANG_THREAD_CONTEXT_H

#include "Types.h"

#include <vector>

namespace Language {

    struct PageTable {
        std::vector<PageAddress> pages;
        Address extent;
        Address capacity;
        PageTable() : extent( 0 ), capacity( 0 ) {};
    };
    // Scope is a list of names corresponding to the current nested scope.
    // The list holds namespace names, procedure names and annonymous block indeces.
    // The scope list is used to uniquely identify variable names.
    typedef std::vector<std::string> Scope;
    // The symbol-table maps fully qualified variable or procedure names to the corresponding descriptors.
    // A name is qualified via its nested scope; e.g., ":function:1:x" is the fully qualified variable name of
    // "x" defined in the second statement block of the the procedure "function" defined in the global scope.
    typedef std::map<std::string,Descriptor> SymbolTable;

    struct ThreadContext {
        Address pc;  // Program counter
        Address sp;  // Stack pointer
        Address ep;  // Expression pointer
        Address ap;  // Argument pointer
        Address fp;  // Frame pointer
        PageTable stack;
        PageTable program;
        PageTable global;
        PageTable string;
        Scope scope;
        SymbolTable symbols;
        ThreadContext() : pc( 0 ), sp( 0 ), ep( 0 ), ap( 0 ), fp( 0 ) {};

    };

    const ThreadContext& ccontext();
    ThreadContext& context();

    struct MemoryUsage {
        Address program;
        Address global;
        Address string;
        MemoryUsage();
    };

    static const Word PageAddressBits( 12 );
    static const Word PageSize( 1 << PageAddressBits );
    static const Word PageAddressMask( PageSize - 1 );

    // Determine number of bytes from address to end of page
    inline Word pageRemainder( const Address& address ) { return( PageSize - (address && PageAddressMask) ); }
    // Address memory via a page table
    PageAddress addressMemory( const PageTable& table, const Address address  );

    // Record current memory usage.
    inline MemoryUsage currentMemoryUsage() { return( MemoryUsage() ); }
    // Recover memory to given usage-point.
    void recoverMemory( const MemoryUsage& usage );

    // Address Program memory (read-execute).
    PageAddress addressProgram( const Address address );
    // Allocate Program memory
    Address allocateProgram( const Word extent );
    // Deallocate Progran memory by decreasing program page-table extent
    void deallocateProgram( const Word extent );

    // Address Heap memory.
    Descriptor* addressGlobal( const Address address );
    // Allocate Heap memory
    Address allocateGlobal( const Word extent );
    // Deallocate Heap memory by decreasing heap page-table extent
    void deallocateGlobal( const Word extent );

    // Address String memory.
    PageAddress addressString( const Address address );
    // Allocate String memory
    Address allocateString( const Word extent );
    // Deallocate String memory by decreasing string page-table extent
    void deallocateString( const Word extent );
    
    // Address Stack memory.
    Descriptor* addressStack( const Offset& offset = 0 );
    // Address local variable.
    Descriptor* addressLocal( const Address& address );
    // Address argument variable.
    Descriptor* addressArgument( const Address& address );

} // namespace Language

#endif // LANG_THREAD_CONTEXT_H
