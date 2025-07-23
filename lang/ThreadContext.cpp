#include "ThreadContext.h"
#include "Monitor.h"

using namespace std;

// ToDo: Consider sharing program, global and string page tables accross threads. Depends on multi-threading model.
// ToDo: Extend memory-usage recovery with defined symbols; e.g., box and unbox. Enables running code in a sandbox.
//       Program memory, symbols, global variables and strings must be recovered.
// ToDo: Account for strings that cross page boundaries...
namespace Language {

    namespace {
        // ToDo: Multi-thread access to thread-context via thread local storage (TLS on windows).
        //       Create single threaad-context for the time being.
        ThreadContext currentContext;
    }

    const ThreadContext& ccontext() { return currentContext; };
    ThreadContext& context() { return currentContext; };

    MemoryUsage::MemoryUsage() {
        auto& cctx( ccontext() );
        program = cctx.program.extent;
        global = cctx.program.extent;
        string = cctx.string.extent;
    }
    MemoryUsage::MemoryUsage( const ThreadContext& cctx ) {
        program = cctx.program.extent;
        global = cctx.program.extent;
        string = cctx.string.extent;
    }
    void recoverMemory( ThreadContext& ctx, const MemoryUsage& usage ) {
        ctx.program.extent = usage.program;
        ctx.program.extent = usage.global;
        ctx.string.extent = usage.string;
    }

    void allocatePage( PageTable& table ) {
        static const char* signature = "void allocatePage( PageTable& table )";
        #ifdef _WIN32
            uint8_t* page = reinterpret_cast<uint8_t*>( _aligned_malloc( PageSize, PageSize ) );
        #elif
            uint8_t* page = reinterpret_cast<uint8_t*>( aligned_alloc( PageSize, PageSize ) );
        #endif
        if (page == nullptr) throw std::string( signature ) + " - Could not allocate page!";
        table.pages.push_back( reinterpret_cast<PageAddress>( page ) );
        table.capacity += PageSize;
    }

    PageAddress addressMemory( const PageTable& table, const Address address  ) {
        static const char* signature = "PageAddress addressMemory( const std::vector<PageAddress>& pageTable, const Address address )";
        Word page = (address >> PageAddressBits);
        #ifdef _DEBUG_INTERPRETER
            if (table.pages.size() <= page) throw std::string( signature ) + " - Page fault at " + to_string( address );
        #endif
        return( table.pages[ page ] + (address & PageAddressMask) );
    }

    Address allocateMemory( PageTable& table, const Word allocation ) {
        Address allocated( table.extent );
        table.extent += allocation;
        while (table.capacity < (table.extent + 1)) allocatePage( table );
        return allocated;
    }

    PageAddress addressProgram( const ThreadContext& cctx, const Address address ) { return addressMemory( cctx.program, address ); }
    Address allocateProgram( ThreadContext& ctx, const Word extent ) { return allocateMemory( ctx.program, extent ); }
    void deallocateProgram( ThreadContext& ctx, const Word extent ) { ctx.program.extent -= extent; }

    Descriptor* addressGlobal( const ThreadContext& cctx, const Address address ) {
        static const char* signature = "PageAddress addressHeap( const Address address )";
        if (monitor( DebugAspects::GlobalAccess )) monitorRecord() << setw( 20 ) << "" << "Global variable access " << address << record<char>;
        return reinterpret_cast<Descriptor*>( addressMemory( cctx.global, address ) );
    }
    Address allocateGlobal( ThreadContext& ctx, const Word extent ) { return allocateMemory( ctx.global, extent ); };
    void deallocateGlobal( ThreadContext& ctx, const Word extent ) { ctx.global.extent -= extent; }
    
    PageAddress addressString( const ThreadContext& cctx, const Address address ) { return addressMemory( cctx.string, address ); }
    Address allocateString( ThreadContext& ctx, const Word extent ) { return allocateMemory( ctx.string, extent ); };
    void deallocateString( ThreadContext& ctx, const Word extent ) { ctx.string.extent -= extent; }

    Descriptor* addressStack( const ThreadContext& cctx, const Offset& offset ) {
        static const char* signature = "Descriptor* addressStack( ThreadContext& context, const Offset& offset )";
        #ifdef _DEBUG_INTERPRETER
            if (cctx.sp < offset) throw std::string( signature ) + " - Invalid stack offset " + std::to_string( offset );
        #endif
        Word page = ((cctx.sp - offset) >> PageAddressBits);
        if (cctx.stack.pages.size() <= page) allocatePage( context().stack );
        if (monitor( DebugAspects::StackAccess )) monitorRecord() << setw( 20 ) << "" << "Stack access " << cctx.sp << " - " << offset << record<char>;
        return reinterpret_cast<Descriptor*>( addressMemory( cctx.stack, (cctx.sp - offset) ) );
    }
    Descriptor* addressLocal( const ThreadContext& cctx, const Address& address ) {
        if (monitor( DebugAspects::LocalAccess )) monitorRecord() << setw( 20 ) << "" << "Local variable access " << address << record<char>;
        return reinterpret_cast<Descriptor*>( addressMemory( cctx.stack, (cctx.fp + address) ) );
    }
    Descriptor* addressArgument( const ThreadContext& cctx, const Address& address ) {
        if (monitor( DebugAspects::ArgumentAccess )) monitorRecord() << setw( 20 ) << "" << "Argument variable access " << address << record<char>;
        return reinterpret_cast<Descriptor*>( addressMemory( cctx.stack, (cctx.ap + address) ) );
    }

} // namespace Language