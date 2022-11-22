#include <stdlib.h>
#include <new>
#include "PagePool.h"

using namespace std;

namespace BTree {

    // ToDo: Lazy read of pages from file.
    // ToDo: Swap pages to/from file to save memory based on usage count.

    // Maintain a pool of fixed size memory pages.
    // Each page in the memory pool is accessed via a PageLink.
    // The use of (32-bit) PageLink links avoids the use of (64-bit) memory pointers, significantly
    // reducing memory usage in paged data structures such as B-trees.
    // The maximum number of pages in a PagePool is limited by the 32 bits of a PageLink.
    // The capacity of paged data structure depends on the size of a page and the use of 32-bit PageLinks.
    // For example, the capacity is ~17.6 TB with 4K pages.

    // Create a PagePool with the given page size.
    PagePool::PagePool( PageSize pageSize ) : capacity( pageSize ) { linkRoot.nullify(); }
    PagePool::~PagePool() {
        for ( int p = (pages.size() - 1); 0 <= p; p-- ) std::free( reinterpret_cast<void*>( pages[ p ] ) );
    };
    // Allocate a page in the memory pool.
    // Returns a PageLink that indexes the allocated page.
    // Allocates and additional page of memory as required.
    PageHeader* PagePool::allocate() {
        static const std::string signature( "PageHeader* PagePool::allocate()" );
        if (freePages.empty()) {
            // No recycled pages available, allocate additional page and add it in the pool...
            PageLink link = PageLink( pages.size() );
            PageHeader* header = reinterpret_cast<PageHeader*>( malloc( capacity ) );
            if (header == nullptr) throw signature + " - Page allocation failed";
            pages.push_back( header );
            header->page = link;
            header->capacity = this->capacity;
            header->free = 0;
            header->modified = 0;
            header->persistent = 0;
            header->recover = 0;
            header->depth = MaxPageDepth;
            header->count = 0;
            header->split = 0;
            return( header );
        }
        // Reuse recycled page
        PageLink link = freePages.back();
        PageHeader* recycledPage = pages[ link.index ];
        recycledPage->free = 0;
        recycledPage->depth = MaxPageDepth;
        recycledPage->count = 0;
        recycledPage->split = 0;
        freePages.pop_back();
        return recycledPage;
    };
    // Free the page indexed by a PageLink.
    void PagePool::free( const PageLink& link ) {
        static const std::string signature( "void PagePool::free( const PageLink& link )" );
        // Recycle page by adding it to the free list
        if (link.null()) throw signature + " - Freeing null page";
        PageLink freedLink = PageLink( link );
        const PageHeader* freedPage = pages[ link.index ];
        if (freedPage->free == 1) throw signature + " - Freeing free page";
        freedPage->free = 1;
        freePages.push_back( link );
    };
    // Return the address of the page indexed by the a PageLink.
    PageHeader* PagePool::reference( const PageLink& link ) const {
        static const std::string signature( "PageHeader* PagePool::reference( const PageLink& link ) const" );
        if (link.null()) { return nullptr; }
        if ( pages.size() <= link.index ) throw signature + " - Invalid pool index";
        PageHeader* page = pages[ link.index ];
        if (page->free == 1) throw signature + " - Referencing free page";
        return( page );
    };

    // Mark a page as modified and queue page for file update.
    // Page will be written to file with next commit() call,
    // unless preceeded by a call to recover().
    void PagePool::modify( const PageHeader& page ) {
        if (page.modified == 0) {
            modifiedPages.push_back( page.page );
            page.modified = 1;
        }
    };

    // Commit all outstanding modify requests by defining new root.
    void PagePool::commit( const PageLink link ) {
        for (int i = 0; i < modifiedPages.size(); i++) {
            const PageHeader* modifiedPage = pages[ modifiedPages[ i ].index ];
            modifiedPage->modified = 0;
        }
        modifiedPages.clear();
        linkRoot = link;
    }

    // Discard all outstanding modify requests by recovering old root.
    // Optionally free all modified pages.
    PageLink PagePool::recover( bool freeModifiedPages ) {
        for (int i = 0; i < modifiedPages.size(); i++) {
            const PageHeader* modifiedPage = pages[ modifiedPages[ i ].index ];
            modifiedPage->modified = 0;
            if ((modifiedPage->free == 0) && freeModifiedPages) free( modifiedPage );
        }
        modifiedPages.clear();
        return( linkRoot );
    }

} // namespace BTree
