#include "Memory.h"
#include "Monitor.h"

// ToDo: Use virtual memory OS calls to allocate page

using namespace std;

namespace Language {

    namespace {
        // The page tables hold addresses of pages in memeory.
        // Page tables grow as memeory is allocated.
        std::vector<PageAddress> programPageTable;
        Address programExtent( 0 );
        std::vector<PageAddress> heapPageTable;
        Address heapExtent( 0 );
        std::vector<PageAddress> stringPageTable;
        Address stringExtent( 0 );
        // Stack page table is accessed via thread context
        // Stack extent defined by thread context stack pointer
    }

    MemoryUsage::MemoryUsage() : program( programExtent ), heap( heapExtent ), string( stringExtent ) {}
    void recoverMemory( const MemoryUsage& usage ) {
        programExtent = usage.program;
        heapExtent = usage.heap;
        stringExtent = usage.string;
    }

    void allocatePage( std::vector<PageAddress>& pageTable ) {
        static const char* signature = "void allocatePage( std::vector<PageAddress>& pageTable )";
        #ifdef _WIN32
            uint8_t* page = reinterpret_cast<uint8_t*>( _aligned_malloc( PageSize, PageSize ) );
        #elif
            uint8_t* page = reinterpret_cast<uint8_t*>( aligned_alloc( PageSize, PageSize ) );
        #endif
        if (page == nullptr) throw string( signature ) + " - Could not allocate page!";
        pageTable.push_back( reinterpret_cast<PageAddress>( page )  );
    }

    PageAddress addressMemory( const std::vector<PageAddress>& pageTable, const Address address  ) {
        static const char* signature = "PageAddress addressMemory( const std::vector<PageAddress>& pageTable, const Address address )";
        Word page = (address >> PageAddressBits);
        #ifdef _DEBUG_INTERPRETER
            if (pageTable.size() <= page) throw std::string( signature ) + " - Page fault at " + to_string( address );
        #endif
        return( pageTable[ page ] + (address & PageAddressMask) );
    }

    Address allocateMemory( std::vector<PageAddress>& pageTable, Address& tableExtent, const Word allocation ) {
        Address allocated( tableExtent );
        tableExtent += allocation;
        while ((pageTable.size() * PageSize) < tableExtent) allocatePage( pageTable );
        return allocated;
    }

        PageAddress addressProgram( const Address address ) { return addressMemory( programPageTable, address ); }
        Address allocateProgram( const Word extent ) { return allocateMemory( programPageTable, programExtent, extent ); }
        void deallocateProgram( const Word extent ) { programExtent -= extent; }
    
        Descriptor* addressGlobal( const Address address ) {
            static const char* signature = "PageAddress addressHeap( const Address address )";
            if (monitor( DebugAspects::GlobalAccess )) monitorRecord() << setw( 20 ) << "" << "Global variable access " << address << record<char>;
            return reinterpret_cast<Descriptor*>( addressMemory( heapPageTable, address ) );
        }
        Address allocateHeap( const Word extent ) { return allocateMemory( heapPageTable, heapExtent, extent ); };
        void deallocateHeap( const Word extent ) { heapExtent -= extent; }
        
        PageAddress addressString( const Address address ) { return addressMemory( stringPageTable, address ); }
        Address allocateString( const Word extent ) { return allocateMemory( stringPageTable, stringExtent, extent ); };
        void deallocateString( const Word extent ) { programExtent -= extent; }

        Descriptor* addressStack( const ThreadContext& context, const Offset& offset ) {
            static const char* signature = "Descriptor* addressStack( ThreadContext& context, const Offset& offset )";
            #ifdef _DEBUG_INTERPRETER
                if (context.sp < offset) throw std::string( signature ) + " - Invalid stack offset " + std::to_string( offset );
            #endif
            Word page = ((context.sp - offset) >> PageAddressBits);
            if (context.stack.size() <= page) allocatePage( const_cast<ThreadContext&>( context ).stack );
            if (monitor( DebugAspects::StackAccess )) monitorRecord() << setw( 20 ) << "" << "Stack access " << offset << record<char>;
            return reinterpret_cast<Descriptor*>( addressMemory( context.stack, (context.sp - offset) ) );
        }
        Descriptor* addressLocal( const ThreadContext& context, const Address& address ) {
            if (monitor( DebugAspects::LocalAccess )) monitorRecord() << setw( 20 ) << "" << "Local variable access " << address << record<char>;
            return reinterpret_cast<Descriptor*>( addressMemory( context.stack, (context.fp + address) ) );
        }
        Descriptor* addressArgument( const ThreadContext& context, const Address& address ) {
            if (monitor( DebugAspects::ArgumentAccess )) monitorRecord() << setw( 20 ) << "" << "Argument variable access " << address << record<char>;
            return reinterpret_cast<Descriptor*>( addressMemory( context.stack, (context.ap + address) ) );
        }

} // namespace Language