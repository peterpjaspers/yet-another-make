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
    // The list holds namespace names, procedure names and annonymous block indexes.
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

    typedef Descriptor(IntrinsicFunction)( ThreadContext& ctx, int arc, Descriptor* argv );

    // Access the current thread context.
    ThreadContext& context();
    // Const access the current thread context.
    const ThreadContext& ccontext();

    struct MemoryUsage {
        Address program;
        Address global;
        Address string;
        MemoryUsage();
        MemoryUsage( const ThreadContext& cctx );
    };

    static const Word PageAddressBits( 12 );
    static const Word PageSize( 1 << PageAddressBits );
    static const Word PageAddressMask( PageSize - 1 );

    // Determine number of bytes from address to end of page
    inline Word pageRemainder( const Address& address ) { return( PageSize - (address && PageAddressMask) ); }
    // Address memory via a page table
    inline PageAddress addressMemory( const PageTable& table, const Address address  );

    // Record current memory usage.
    inline MemoryUsage currentMemoryUsage() { return( MemoryUsage() ); }
    inline MemoryUsage currentMemoryUsage( const ThreadContext& cctx ) { return( MemoryUsage( cctx ) ); }
    // Recover memory to given usage-point.
    void recoverMemory( ThreadContext& ctx, const MemoryUsage& usage );
    inline void recoverMemory( const MemoryUsage& usage ) { recoverMemory( context(), usage ); };

    // Address Program memory (read-execute).
    PageAddress addressProgram( const ThreadContext& cctx, const Address address );
    inline PageAddress addressProgram( const Address address ) { return addressProgram( ccontext(), address ); }
    // Allocate Program memory
    Address allocateProgram( ThreadContext& ctx, const Word extent );
    inline Address allocateProgram( const Word extent ) { return allocateProgram( context(), extent ); }
    // Deallocate Progran memory by decreasing program page-table extent
    void deallocateProgram( ThreadContext& ctx, const Word extent );
    inline void deallocateProgram( const Word extent ) { deallocateProgram( context(), extent ); }

    // Address Heap memory.
    Descriptor* addressGlobal( const ThreadContext& cctx, const Address address );
    inline Descriptor* addressGlobal( const Address address ) { return addressGlobal( ccontext(), address ); };
    // Allocate Heap memory
    Address allocateGlobal( ThreadContext& ctx, const Word extent );
    inline Address allocateGlobal( const Word extent ) { return allocateGlobal( context(), extent ); }
    // Deallocate Heap memory by decreasing heap page-table extent
    void deallocateGlobal( ThreadContext& ctx, const Word extent );
    inline void deallocateGlobal( const Word extent ) { deallocateGlobal( context(), extent ); }

    // Address String memory.
    PageAddress addressString( const ThreadContext& cctx, const Address address );
    inline PageAddress addressString( const Address address ) { return addressString( ccontext(), address ); }
    // Allocate String memory
    Address allocateString( ThreadContext& ctx, const Word extent );
    inline Address allocateString( const Word extent ) { return allocateString( context(), extent ); }
    // Deallocate String memory by decreasing string page-table extent
    void deallocateString( ThreadContext& ctx, const Word extent );
    inline void deallocateString( const Word extent ) { deallocateString( context(), extent ); }
    
    // Address Stack memory.
    Descriptor* addressStack( const ThreadContext& cctx, const Offset& offset = 0 );
    inline Descriptor* addressStack( const Offset& offset = 0 ) { return addressStack( ccontext(), offset ); }
    // Address local variable.
    Descriptor* addressLocal( const ThreadContext& cctx, const Address& address );
    inline Descriptor* addressLocal( const Address& address ) { return addressLocal( ccontext(), address ); }
    // Address argument variable.
    Descriptor* addressArgument( const ThreadContext& cctx, const Address& address );
    inline Descriptor* addressArgument( const Address& address ) { return addressArgument( ccontext(), address ); }

} // namespace Language

#endif // LANG_THREAD_CONTEXT_H
