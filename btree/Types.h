#ifndef BTREE_TYPES_H
#define BTREE_TYPES_H

#include <cstdint>
#include <iostream>
#include <iostream>

namespace BTree {

    typedef std::uint16_t PageIndex;
    typedef std::uint16_t PageDepth;
    typedef std::uint16_t PageSize;
    typedef std::int32_t  KeyCompare;

    const PageSize MinPageSize = 128;
    const PageSize MaxPageSize = 32768;
    const unsigned int MaxPagePoolIndex = 0xFFFFFFFF;
    const unsigned int MaxPageDepth = 0x0FFF;

    // Reference to a page in a paged memory pool.
    // PageLinks are 32-bit values that index a page in the ppol.
    // Access to memory in a PagePool is exclusively via PageLinks as opposed to direct
    // memory access via 64-bit pointers significanlty reducing memory usage in paged data structures
    // suzh as B-trees.
    class PageLink {
    public:
        uint32_t index; // Index of a page in the pool
        // inline PageLink() : index( MaxPagePoolIndex ) {}
        inline PageLink() {}
        inline PageLink( const PageLink& link ) { index = link.index; }
        inline PageLink( unsigned int index ) { this->index = index; }
        inline PageLink& operator= ( const PageLink& link ) { index = link.index; return( *this ); }
        inline void nullify() { index = MaxPagePoolIndex; }
        inline bool null() const { return( index == MaxPagePoolIndex ); }
        friend bool operator== ( const PageLink& a, const PageLink& b );
        friend bool operator!= ( const PageLink& a, const PageLink& b );
        friend bool operator<  ( const PageLink& a, const PageLink& b );
        friend std::ostream & operator<<( std::ostream & stream, PageLink const & link );
    };

    inline bool operator== ( const PageLink& a, const PageLink& b ) { return( a.index == b.index ); }
    inline bool operator!= ( const PageLink& a, const PageLink& b ) { return( a.index != b.index ); }
    inline bool operator<  ( const PageLink& a,const PageLink& b )  { return( a.index <  b.index ); };

    struct PageHeader {
        PageLink page;                       // Link to this page (self reference)
        mutable unsigned int free : 1;       // Page is free (not in use)
        mutable unsigned int modified : 1;   // Page is modified (changed after last commit or allocate)
        mutable unsigned int persistent : 1; // Page is present in persistent store (previously consolidated)
        mutable unsigned int recover : 1;    // Page may need to be recovered (modified after last commit)
        unsigned int depth : 12;             // The depth in the BTree of this Page, 0 for leaf pages.
        PageSize capacity;                   // Page capacity in bytes
        PageSize count;                      // Number of key-value pairs in Page
        PageSize split;                      // Size of split value (0 for no split, 1 for scalar split, array size otherwise)
    };

    inline bool operator== ( const PageHeader& a, const PageHeader& b ) {
        return(
            (a.page == b.page ) &&
            (a.free == b.free ) &&
            (a.depth == b.depth ) &&
            (a.capacity == b.capacity ) &&
            (a.count == b.count )
        );
    }
    inline bool operator!= ( const PageHeader& a, const PageHeader& b ) {
        return(
            (a.page != b.page ) ||
            (a.free != b.free ) ||
            (a.depth != b.depth ) ||
            (a.capacity != b.capacity ) ||
            (a.count != b.count )
        );
    }

    template <class K, class V, bool KA, bool VA> class Page;

} // namespace BTree

#endif // BTREE_TYPES_H