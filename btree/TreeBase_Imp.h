#ifndef BTREE_TREE_BASE_IMP_H
#define BTREE_TREE_BASE_IMP_H

#include "TreeBase.h"

namespace BTree {

    template< class K >
    inline int defaultCompareScalar( const K& a, const K& b ) {
        return( std::less<K>()( a, b ) ? -1 : ( std::less<K>()( b, a ) ? +1 : 0 ) );
    };
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

    TreeBase::TreeBase( PagePool& pagePool, PageHeader* page, UpdateMode updateMode ) :
        pool( pagePool ), root( page ), mode( deriveMode( updateMode, pagePool ) ), index( FreeStandingTree ), stats( nullptr )
    {}
    // Derive BTree update mode as function of requested mode and page pool persistence.
    inline UpdateMode TreeBase::deriveMode( UpdateMode mode, PagePool& pool ) {
        if (mode != Auto) return mode;
        if (pool.persistent()) return PersistentTransaction;
        return InPlace;
    }
    void TreeBase::commit() const {
        if (stats) stats->commits += 1;
        static const char* signature = "void TreeBase::commit() const";
        if (index != FreeStandingTree) throw std::string( signature ) + " - Cannot commit single tree in forest";
        pool.commit( root->page, stats );
    }
    void TreeBase::recover() {
        if (stats) stats->recovers += 1;
        static const char* signature = "void TreeBase::recover()";
        if (index != FreeStandingTree) throw std::string( signature ) + " - Cannot recover single tree in forest";
        PageLink link( pool.recover( (mode == PersistentTransaction), stats ) );
        recoverTree( link );
    }
    bool TreeBase::enableStatistics( const BTreeStatistics* initial ) const {
        if (stats != nullptr) return false;
        stats = new BTreeStatistics();
        if (initial != nullptr) *stats = *initial;
        return true;
    }
    bool TreeBase::disableStatistics( BTreeStatistics* current ) const {
        if (stats == nullptr) return false;
        if (current != nullptr) *current = *stats;
        delete stats;
        stats = nullptr;
        return true;
    }
    bool TreeBase::clearStatistics() const {
        if (stats == nullptr) return false;
        stats->clear();
        return true;
    }
    inline bool TreeBase::statisticsEnabled() const {
        if (stats == nullptr) return false;
        return true;            
    }
    bool TreeBase::statistics( BTreeStatistics& current ) const {
        if (stats == nullptr) return false;
        current = *stats;
        return true;
    }
} // namespace BTree

#endif // BTREE_TREE_BASE_IMP_H
