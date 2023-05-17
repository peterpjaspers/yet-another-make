#ifndef PAGE_POOL_IMP_H
#define PAGE_POOL_IMP_H

#include "PagePool.h"

namespace BTree {

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
    inline PageHeader* PagePool::clean() {
        currentRoot = commitRoot();
        return( currentRoot );
    };
    inline PageHeader* PagePool::commitRoot() const { return reference( commitLink ); };
    inline PageLink PagePool::rootLink() const { return commitLink; };

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
