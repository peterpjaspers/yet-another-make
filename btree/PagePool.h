#ifndef PAGE_POOL_H
#define PAGE_POOL_H

#include <vector>
#include "Types.h"
#include "Page.h"

namespace BTree {

    // A PagePool maintains a list of fixed size pages in which a B-Tree is stored.
    // Pages are allocated on demand and recycled via a list of free pages.
    // A page is referred to by a PageLink which is in effect an index in the
    // list (vector) of pages.
    class PagePool {
    protected:
        // The (fized size) capacity of a page in 8-bit (uint8_t) bytes.
        // All pages in pool have the same capacity.
        PageSize capacity;
        // All pages in pool, list increases in size when a new page is allocated.
        std::vector<PageHeader*> pages;
        // List of freed pages. List operates as a stack, the last to be freed is the first
        // to be reused when a page is allocated. If the free pages list is empty, a page
        // allocation results in an additional page in the pages list.
        std::vector<PageLink> freePages;
        // List of modified pages.
        // Modified page semantics depends on update mode in effect.
        std::vector<PageLink> modifiedPages;
        // Header to current (not necessarily commited) B-tree root page (not interpreted by PagePool)
        PageHeader* currentRoot;
        // Link to (previously) commited B-tree root page 
        PageLink commitLink;
    public:
        PagePool() =delete;
        // Create a PagePool with pages of the given capacity.
        PagePool( PageSize pageCapacity );
        ~PagePool();
        PageSize pageCapacity() const;
        // Allocate a page.
        PageHeader* allocate();
        // Return total number of pages.
        uint32_t size() const;
        // Return number of free pages.
        uint32_t sizeFreed() const;
        // Return number of modified pages.
        uint32_t sizeModified() const;
        // Return number of recover pages (in a persistent pool).
        virtual uint32_t sizeRecover() const;
        // Reference a page.
        // Throws an exception if the page is free or the link is is invalid.
        PageHeader* reference( const PageLink& link ) const;
        // Access a (possibly free) page.
        // Throws an exception if the link is is invalid.
        const PageHeader& access( const PageLink& link ) const;
        // Free a page.
        void free( const PageLink& link );
        void free( const PageHeader& header );
        void free( const PageHeader* header );
        // Check if a link is valid; i.e., actually references a page in the pool.
        bool valid( const PageLink& link );
        // Check if a page is valid.
        bool valid( const PageHeader& header );
        // Mark page as modified.
        virtual void modify( const PageHeader& page );
        // Determine if this pool is persistent.
        virtual bool persistent() const;
        // Consolidate the current state of all modified pages.
        virtual void commit( const PageLink link, BTreeStatistics* stats = nullptr );
        // Recover the state of all pages to that of the previous commit.
        virtual PageLink recover( bool freeModifiedPages = false, BTreeStatistics* stats = nullptr );
        virtual void recover( const PageHeader& page, bool reuse = false );
        virtual PageHeader* clean();
        // Return page header of last commited root
        PageHeader* commitRoot() const;

        // Allocate a new Page
        template< class K, class V, bool KA, bool VA >
        Page<K,V,KA,VA>* page( const PageDepth depth );
        // Access a Page given a pointer to its PageHeader
        template< class K, class V, bool KA, bool VA >
        Page<K,V,KA,VA>* page( const PageHeader* header ) const;
        // Access a Page given its PageLink
        template< class K, class V, bool KA, bool VA >
        Page<K,V,KA,VA>* page( const PageLink& link ) const;
    };

} // namespace BTree

#include "PagePool_Imp.h"

#endif // PAGE_POOL_H

