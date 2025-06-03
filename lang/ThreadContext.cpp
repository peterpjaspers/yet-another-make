#include "ThreadContext.h"
#include "Monitor.h"

using namespace std;

// ToDo: Consider sharing program, global and string page tables accross threads. Depends on multi-threading model.
// ToDo: Provide all functions that access the thread-context with a version that has the thread-context as the first argument.
//       This will reduce the number of calls required to access the thread-context.
namespace Language {

    namespace {
        // ToDo: Multi-thread access to thread-context via thread local storage (TLS on windows).
        //       Create single threaad-context for the time being.
        ThreadContext currentContext;
    }

    const ThreadContext& ccontext() { return currentContext; };
    ThreadContext& context() { return currentContext; };

    MemoryUsage::MemoryUsage() {
        auto& ctx( ccontext() );
        program = ctx.program.extent;
        global = ctx.program.extent;
        string = ctx.string.extent;
    }
    void recoverMemory( const MemoryUsage& usage ) {
        auto& ctx( context() );
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

    PageAddress addressProgram( const Address address ) { return addressMemory( ccontext().program, address ); }
    Address allocateProgram( const Word extent ) { return allocateMemory( context().program, extent ); }
    void deallocateProgram( const Word extent ) { context().program.extent -= extent; }

    Descriptor* addressGlobal( const Address address ) {
        static const char* signature = "PageAddress addressHeap( const Address address )";
        if (monitor( DebugAspects::GlobalAccess )) monitorRecord() << setw( 20 ) << "" << "Global variable access " << address << record<char>;
        return reinterpret_cast<Descriptor*>( addressMemory( ccontext().global, address ) );
    }
    Address allocateGlobal( const Word extent ) { return allocateMemory( context().global, extent ); };
    void deallocateGlobal( const Word extent ) { context().global.extent -= extent; }
    
    PageAddress addressString( const Address address ) { return addressMemory( ccontext().string, address ); }
    Address allocateString( const Word extent ) { return allocateMemory( context().string, extent ); };
    void deallocateString( const Word extent ) { context().string.extent -= extent; }

    Descriptor* addressStack( const Offset& offset ) {
        static const char* signature = "Descriptor* addressStack( ThreadContext& context, const Offset& offset )";
        auto& ctx( ccontext() );
        #ifdef _DEBUG_INTERPRETER
            if (ctx.sp < offset) throw std::string( signature ) + " - Invalid stack offset " + std::to_string( offset );
        #endif
        Word page = ((ctx.sp - offset) >> PageAddressBits);
        if (ctx.stack.pages.size() <= page) allocatePage( context().stack );
        if (monitor( DebugAspects::StackAccess )) monitorRecord() << setw( 20 ) << "" << "Stack access " << offset << record<char>;
        return reinterpret_cast<Descriptor*>( addressMemory( ctx.stack, (ctx.sp - offset) ) );
    }
    Descriptor* addressLocal( const Address& address ) {
        auto& ctx( ccontext() );
        if (monitor( DebugAspects::LocalAccess )) monitorRecord() << setw( 20 ) << "" << "Local variable access " << address << record<char>;
        return reinterpret_cast<Descriptor*>( addressMemory( ctx.stack, (ctx.fp + address) ) );
    }
    Descriptor* addressArgument( const Address& address ) {
        auto& ctx( ccontext() );
        if (monitor( DebugAspects::ArgumentAccess )) monitorRecord() << setw( 20 ) << "" << "Argument variable access " << address << record<char>;
        return reinterpret_cast<Descriptor*>( addressMemory( ctx.stack, (ctx.ap + address) ) );
    }

} // namespace Language