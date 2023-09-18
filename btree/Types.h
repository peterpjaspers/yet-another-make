#ifndef BTREE_TYPES_H
#define BTREE_TYPES_H

#include <cstdint>
#include <iostream>

namespace BTree {

    typedef std::uint16_t PageIndex;
    typedef std::uint16_t PageDepth;
    typedef std::uint16_t PageSize;
    typedef std::int32_t  KeyCompare;

    const PageSize MinPageSize = 128;
    const PageSize MaxPageSize = 32768;
    const std::uint32_t MaxPagePoolIndex = UINT32_MAX;
    const PageDepth MaxPageDepth = 0x0FFF;

    // Reference to a page in a paged memory pool.
    // PageLinks are 32-bit values that index a page in the ppol.
    // Access to memory in a PagePool is exclusively via PageLinks as opposed to direct
    // memory access via 64-bit pointers significanlty reducing memory usage in paged data structures
    // such as B-trees. 
    class PageLink {
    public:
        uint32_t index; // Index of a page in the pool
        inline PageLink() {}
        inline PageLink( const PageLink& link ) { index = link.index; }
        inline PageLink( unsigned int index ) { this->index = index; }
        inline PageLink& operator= ( const PageLink& link ) { index = link.index; return( *this ); }
        inline PageLink& nullify() { index = MaxPagePoolIndex; return( *this ); }
        inline bool null() const { return( index == MaxPagePoolIndex ); }
        friend bool operator== ( const PageLink& a, const PageLink& b );
        friend bool operator!= ( const PageLink& a, const PageLink& b );
        friend bool operator<  ( const PageLink& a, const PageLink& b );
        friend bool operator<= ( const PageLink& a, const PageLink& b );
        friend bool operator>  ( const PageLink& a, const PageLink& b );
        friend bool operator>= ( const PageLink& a, const PageLink& b );
        friend std::ostream & operator<<( std::ostream & stream, PageLink const & link );
    };
    
    static const PageLink nullPage( MaxPagePoolIndex );

    inline bool operator== ( const PageLink& a, const PageLink& b ) { return( a.index == b.index ); }
    inline bool operator!= ( const PageLink& a, const PageLink& b ) { return( a.index != b.index ); }
    inline bool operator<  ( const PageLink& a, const PageLink& b ) { return( a.index <  b.index ); };
    inline bool operator<= ( const PageLink& a, const PageLink& b ) { return( a.index <=  b.index ); };
    inline bool operator>  ( const PageLink& a, const PageLink& b ) { return( a.index >  b.index ); };
    inline bool operator>= ( const PageLink& a, const PageLink& b ) { return( a.index >=  b.index ); };

    struct PageHeader {
        PageLink page;                   // Link to this page (self reference)
        mutable uint16_t free : 1;       // Page is free (not in use)
        mutable uint16_t modified : 1;   // Page is modified (changed after last commit or allocate)
        mutable uint16_t persistent : 1; // Page is defined in persistent store (previously commited)
        mutable uint16_t recover : 1;    // Page may need to be recovered (modified after last commit)
        mutable uint16_t stored : 1;     // Page present in persistent store (either free or persistent)
        uint16_t depth : 12;             // The depth in the BTree of this Page, 0 for leaf pages.
        PageSize capacity;               // Page capacity in bytes
        PageSize count;                  // Number of key-value pairs in Page
        PageSize split;                  // Size of split value
        // 0 for no split, 1 for fixed size split, variable size value element count (not sizeof) otherwise
    };

    class PagePool;
    template< class K, class V, bool AK, bool AV > class Page;
    template< class K, class V > class Tree;

    // Fixed size element type of (variable length, unbounded) array template argument.
    // Evaluates to type of first dimension element if template argument is an array.
    // Evaluates to type of template argument otherwise.
    // Evaluates to bound (fixed size) array for multi-dimensional arrays.
    template< class T >
    using B = typename std::remove_extent<T>::type;

    // Determine array length variablilty through template instantiation.
    // For multi-dimentsional arrays, succeeds or fails if single first dimension is unbound.
    // Note that only first dimension (of a C++ array) may be unbound.
    // Template A instantiation succeeds for variable length arrays.
    // Template S instantiation succeeds for fixed length arrays or non-array types.
    template< class T >
    constexpr bool A = bool((0 < std::rank_v<T>) && (std::extent_v<T,0> == 0));
    template< class T >
    constexpr bool S = bool((std::rank_v<T> == 0) || (0 < std::extent_v<T,0>));

    // B-Tree statistics consist of a collection of function counters.
    // Counters are updated when the corresponding B-Tree function is executed.
    // The gathering of statistics on a B-Tree is controlled via the functions:
    //      enableStatistics
    //      disableStatistics
    //      clearStatistics
    //      statisticsEnabled
    //      statistics
    // See BTree::TreeBase for further information.
    struct BTreeStatistics {
        uint32_t    insertions;
        uint32_t    retrievals;
        uint32_t    replacements;
        uint32_t    removals;
        uint32_t    finds;
        uint32_t    grows;
        uint32_t    pageAllocations;
        uint32_t    pageFrees;
        uint32_t    mergeAttempts;
        uint32_t    pageMerges;
        uint32_t    pageShifts;
        uint32_t    rootUpdates;
        uint32_t    splitUpdates;
        uint32_t    commits;
        uint32_t    recovers;
        uint32_t    pageWrites;
        uint32_t    pageReads;
        BTreeStatistics();
        // Set all counters to zero.
        BTreeStatistics* clear();
        // Assign counters to given counter values.
        BTreeStatistics* operator=( const BTreeStatistics& stats );
        // Increment counters with given counter values.
        BTreeStatistics* operator+( const BTreeStatistics& stats );
    };

} // namespace BTree

#endif // BTREE_TYPES_H