#ifndef PAGE_POOL_H
#define PAGE_POOL_H

#include <vector>
#include <new>
#include "Types.h"
#include "Page.h"

namespace BTree {

class PagePool;

    class PagePool {
    protected:
        // The (fized size) capacity of a page in 8-bit (uint8_t) bytes.
        // All pages in pool have the same capacity.
        PageSize capacity;
        // All pages in pool, list increases in size when a new page is allocated.
        std::vector<PageHeader*> pages;
        // List of freed pages. List operates as a stack, the last to be freed is the first
        // to be reused when a page is allocated. If the free pages list is empty, a page
        // allocation results in aadditional page in the pages list.
        std::vector<PageLink> freePages;
        // List of modified pages.
        // Modified page semantics depends on update mode in effect.
        std::vector<PageLink> modifiedPages;
        // Link to the root of the B-tree (not interpreted by PagePool)
        PageLink linkRoot;
    public:
        PagePool() =delete;
        PagePool( PageSize pageCapacity );
        ~PagePool();
        PageSize pageCapacity() const;
        PageHeader* allocate();
        PageHeader* reference( const PageLink& link ) const;
        const PageHeader& access( const PageLink& link ) const;
        void free( const PageLink& link );
        void free( const PageHeader& header );
        void free( const PageHeader* header );
        bool valid( const PageLink& link );
        bool valid( const PageHeader& header );
        void modify( const PageHeader& page );
        virtual bool persistent() const;
        virtual void recover( const PageHeader& page, bool reuse = false );
        virtual void commit( const PageLink link );
        virtual PageLink recover( bool freeModifiedPages = false );
        PageHeader* root() const;
        PageLink rootLink() const;

        // Generate a new Page
        template< class K, class V, bool KA, bool VA >
        Page<K,V,KA,VA>* page( const PageDepth depth );
        // Access a Page given a pointer to its PageHeader
        template< class K, class V, bool KA, bool VA >
        Page<K,V,KA,VA>* page( const PageHeader* header ) const;
        // Access a Page given its PageLink
        template< class K, class V, bool KA, bool VA >
        Page<K,V,KA,VA>* page( const PageLink& link ) const;
};

    inline PageSize PagePool::pageCapacity() const { return capacity; };
    inline const PageHeader& PagePool::access( const PageLink& link ) const { return *pages[ link.index ]; };
    inline void PagePool::free( const PageHeader& header ) { free( header.page ); }
    inline void PagePool::free( const PageHeader* header ) { free( header->page ); }
    inline bool PagePool::valid( const PageLink& link ) { return( link.index < pages.size() ); }
    inline bool PagePool::valid( const PageHeader& header ) {
        return( (header.free == 0) && (header.depth != MaxPageDepth) && (header.page.index < pages.size()) );
    }
    inline bool PagePool::persistent() const { return false; };
    inline void PagePool::recover( const PageHeader& page, bool reuse ) {};
    inline PageHeader* PagePool::root() const { return reference( linkRoot ); };
    inline PageLink PagePool::rootLink() const { return linkRoot; };

    template< class K, class V, bool KA, bool VA >
    inline Page<K,V,KA,VA>* PagePool::page( const PageDepth depth ) {
        return( new( allocate() ) Page<K,V,KA,VA>( depth ) );
    };
    template< class K, class V, bool KA, bool VA >
    inline Page<K,V,KA,VA>* PagePool::page( const PageHeader* header ) const {
        return( reinterpret_cast<Page<K,V,KA,VA>*>( const_cast<PageHeader*>( header ) ) );
    };
    template< class K, class V, bool KA, bool VA >
    inline Page<K,V,KA,VA>* PagePool::page( const PageLink& link ) const {
        return( page<K,V,KA,VA>( reference( link ) ) );
    };

} // namespace BTree

#endif // PAGE_POOL_H
