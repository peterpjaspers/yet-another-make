#ifndef PAGE_POOL_IMP_H
#define PAGE_POOL_IMP_H

#include <new>
#include "PagePool.h"

namespace BTree {

    inline PageSize PagePool::pageCapacity() const { return capacity; };
    inline uint32_t PagePool::size() const { return static_cast<uint32_t>(pages.size()); };
    inline uint32_t PagePool::sizeFreed() const { return static_cast<uint32_t>(freePages.size()); };
    inline uint32_t PagePool::sizeModified() const { return static_cast<uint32_t>(modifiedPages.size()); }
    inline  uint32_t PagePool::sizeRecover() const { return 0; };
    inline const PageHeader& PagePool::access( const PageLink& link ) const { return *pages[ link.index ]; };
    inline void PagePool::free( const PageHeader& header ) { free( header.page ); }
    inline void PagePool::free( const PageHeader* header ) { free( header->page ); }
    inline bool PagePool::valid( const PageLink& link ) { return( link.index < pages.size() ); }
    inline bool PagePool::valid( const PageHeader& header ) {
        return(
            (header.free == 0) &&
            (header.depth != MaxPageDepth) &&
            (header.page.index < pages.size()) &&
            (&header == &access(header.page))
        );
    }
    inline bool PagePool::persistent() const { return false; };
    inline void PagePool::recover( const PageHeader& page, bool reuse ) { if (reuse) free( page ); };
    inline PageHeader* PagePool::clean() {
        currentRoot = commitRoot();
        return( currentRoot );
    };
    inline PageHeader* PagePool::commitRoot() const { return reference( commitLink ); };

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

#endif // PAGE_POOL_IMP_H
