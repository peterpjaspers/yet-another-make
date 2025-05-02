#ifndef LANG_MEMORY_H
#define LANG_MEMORY_H

#include "Types.h"

#include <vector>
#include <string>
#include <cstdlib>

namespace Language {

    static const Word PageAddressBits( 12 );
    static const Word PageSize( 1 << PageAddressBits );
    static const Word PageAddressMask( PageSize - 1 );

    // Determine number of bytes from address to end of page
    inline Word pageRemainder( const Address& address ) { return( PageSize - (address && PageAddressMask) ); }
    // Allocate a new page to a page table.
    // Pages are aligned in memory enabling address masking to locate page.
    void allocatePage( std::vector<PageAddress>& pageTable );
    // Address page memory corresponding to address
    PageAddress addressMemory( const std::vector<PageAddress>& pageTable, const Address address  );
    // Allocate memory in a page-table by increasing table extent
    Address allocateMemory( std::vector<PageAddress>& pageTable, Address& tableExtent, const Word allocation );

    struct MemoryUsage {
        Address program;
        Address heap;
        Address string;
        MemoryUsage();
    };
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
    Address allocateHeap( const Word extent );
    // Deallocate Heap memory by decreasing heap page-table extent
    void deallocateHeap( const Word extent );

    // Address String memory.
    PageAddress addressString( const Address address );
    // Allocate String memory
    Address allocateString( const Word extent );
    // Deallocate String memory by decreasing string page-table extent
    void deallocateString( const Word extent );
    
    // Address Stack memory.
    Descriptor* addressStack( const ThreadContext& context, const Offset& offset = 0 );
    // Address local variable.
    Descriptor* addressLocal( const ThreadContext& context, const Address& address );
    // Address argument variable.
    Descriptor* addressArgument( const ThreadContext& context, const Address& address );

} // namespace Language

#endif // LANG_MEMORY_H