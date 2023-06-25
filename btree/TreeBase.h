#ifndef BTREE_TREE_BASE_H
#define BTREE_TREE_BASE_H

#include "PagePool.h"
#include <stdlib.h>

namespace BTree {

    // Default scalar key comparison function making use of less<T> on type of key.
    template< class K >
    inline int defaultCompareScalar( const K& a, const K& b ) {
        return( std::less<K>()( a, b ) ? -1 : ( std::less<K>()( b, a ) ? +1 : 0 ) );
    };
    // Default array key comparison function making use of less<T> on type of key array elements.
    template< class K >
    inline int defaultCompareArray( const K* a, PageSize na, const K* b, PageSize nb ) {
        PageSize n = (( na < nb ) ? na : nb );
        for (PageSize i = 0; i < n; i++) {
            if (std::less<K>()( a[ i ], b[ i ] )) {
                return -1;
            } else if (std::less<K>()( b[ i ], a[ i ] )) {
                return +1;
            }
        }
        return( ( na < nb ) ? -1 : ( (nb < na) ? +1 : 0 ) );
    };

    // Updates on a BTree can be either in-place or with copy-on-update behavior.
    // Modifications to a BTree may exhibit in-place or transcation behavior.
    // Transactions require copy-on-update for all page modifications.
    // For transaction behaviour:
    //   Modifications are consolidated with a call to commit(), transaction succeeds.
    //   Modifications are discarded with a call to recover(), transaction fails.
    // During a transaction modified pages are registered with a call to modify() and
    // pages to be recoverd (i.e., copied-on-update) are registered with a call to recover().
    // Pages copied-on-update with a persistent pool may be reused reducing memory footprint
    // (particularly for large transactions).
    enum UpdateMode {
        Auto = 0,                  // Infered from pool.persistent() (false -> InPlace, true -> PersistentTransaction)
        InPlace = 1,               // No copy-on-update behaviour (default for non-persistent pool)
        MemoryTransaction = 2,     // copy-on-update behaviour without memory reuse
        PersistentTransaction = 3  // copy-on-update behaviour with memory reuse (default for persistent pool)
    };

    // A tree index identifies a B-Tree in a Forest.
    // If a B-Tree does not reside in a Forest, it is a free-standing tree.
    typedef std::uint32_t TreeIndex;
    static const TreeIndex FreeStandingTree = UINT32_MAX;
    static const TreeIndex TreeIndexMax = ((1 << 30) - 1);

    // Access to pages of a B-Tree in a PagePool.
    // The B-Tree can be composed by starting at the root page.
    // This requires knowledge of B-Tree page mappings derived from the
    // B-Tree template types for keys (K) and values (V).
    // A collection of B-Trees (Forest) can be mapped to a shared PagePool enabling
    // transaction semantics (commit and recover) on the collection of B-Trees.
    
    class TreeBase {
    protected:
        PagePool& pool;     // The PagePool in which this B-Tree resides
        PageHeader* root;   // The root Page of the B-Tree
        UpdateMode mode;    // The Page update mnode to be enforced
        TreeIndex index;    // The index of the B-Tree in a forest (if not free standing)
        TreeBase( PagePool& pagePool, PageHeader* page, UpdateMode updateMode );
        UpdateMode deriveMode( UpdateMode mode, PagePool& pool );
        virtual void recoverTree( PageLink link ) = 0;
        friend class Trail;
        friend class Forest;
    public:
        // Return depth of B-tree, an indication of the log(N) complexity of B-tree operations.
        PageDepth depth() const;
        // Consolidate all Page modifications.
        void commit() const;
        // Restore all Pages to last consolidated state.
        void recover();
        // Return PageLink of B-Tree root
        PageLink rootLink() const;
    };

    TreeBase::TreeBase( PagePool& pagePool, PageHeader* page, UpdateMode updateMode ) :
        pool( pagePool ), root( page ), mode( deriveMode( updateMode, pagePool ) ), index( FreeStandingTree )
    {}
    // Derive BTree update mode as function of requested mode and page pool persistence.
    UpdateMode TreeBase::deriveMode( UpdateMode mode, PagePool& pool ) {
        if (mode != Auto) return mode;
        if (pool.persistent()) return PersistentTransaction;
        return InPlace;
    }
    PageDepth TreeBase::depth() const { return( root->depth ); }
    void TreeBase::commit() const {
        static const char* signature = "void TreeBase::commit() const";
        if (index != FreeStandingTree) throw std::string( signature ) + " - Cannot commit single tree in forest";
        pool.commit( root->page );
    }
    void TreeBase::recover() {
        static const char* signature = "void TreeBase::recover()";
        if (index != FreeStandingTree) throw std::string( signature ) + " - Cannot recover single tree in forest";
        PageLink link( pool.recover( mode == PersistentTransaction ) );
        recoverTree( link );
    }
    PageLink TreeBase::rootLink() const { return root->page; }

} // namespace BTree

#include "Trail.h"

#endif // BTREE_TREE_BASE_H