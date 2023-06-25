#ifndef BTREE_TREE_H
#define BTREE_TREE_H

#include <stdlib.h>
#include <cstdint>
#include <string.h>
#include <sstream>
#include <iostream>
#include <iomanip>

#include "TreeBase.h"
#include "CompareKey.h"

// ToDo: Align with C++ map (method naming and interface in general)
// ToDo: Fix problem with orphaned pages
// ToDo: Provide function to compact B-Tree to contiguous pages (reducing memory usage and persistent file size)

namespace BTree {

    template< class KT, class VT, class RT > class BTreeIterator;

    static const float LowPageThreshold = 0.4;
    static const float HighPageThreshold = 0.9;

    // B-Tree with template arguments for key (K) and value (V).
    //
    // A B-Tree maps keys to values.
    //
    // For example:
    //   Tree< int, float > - maps integer keys to float values.
    // Either or both arguments may be arrays.
    //   Tree< char[], float > - maps C-string keys to float values.
    //   Tree< int, char[] > - maps integer keys to C-string values.
    //   Tree< char[], uint16_t[] > - maps C-string keys to arrays of 16-bit unsigned integers.
    //
    // Performance cost of an poeration on the B-Tree is typically O(logN(n)) where N is the
    // average number of entries stored in a Page.
    //
    template< class K, class V > class Tree : public TreeBase {
    private:
        // The (user defined) comparison function on keys.
        // Defaults to use of operator<() on key values.
        union {
            int (*scalar)( const B<K>&, const B<K>& );
            int (*array)( const B<K>*, PageSize, const B<K>*, PageSize );
        } compare;
        template< class KT, class VT, class RT >
        friend class BTreeIterator;
        template< class KT, class VT >
        friend std::ostream & operator<<( std::ostream & o, const Tree<KT,VT>& tree );
    public:
        typedef int (*ScalarKeyCompare)( const K&, const K& );
        typedef int (*ArrayKeyCompare)( const B<K>*, PageSize, const B<K>*, PageSize );
        Tree() = delete;
        template< class KT = K, class VT = V >
        Tree(
            std::enable_if_t<(S<KT>),PagePool>& pagePool,
            ScalarKeyCompare compareKey = defaultCompareScalar<KT>,
            UpdateMode updateMode = UpdateMode::Auto
        ) : Tree<KT,VT>( pagePool, compareKey, updateMode, pagePool.clean() ) {}
        template< class KT = K, class VT = V >
        Tree(
            std::enable_if_t<(A<KT>),PagePool>& pagePool,
            ArrayKeyCompare compareKey = defaultCompareArray<B<KT>>,
            UpdateMode updateMode = UpdateMode::Auto
        ) : Tree<KT,VT>( pagePool, compareKey, updateMode, pagePool.clean() ) {}
        ~Tree() { pool.recover(); freeAll( *root, true ); }
        // Insert a key-value entry in the B-Tree.
        // Returns true if the value was inserted, false if the key was already present.
        // O(log(n)) complexity.
        template< class KT = K, class VT = V, std::enable_if_t<(S<KT>&&S<VT>),bool> = true >
        bool insert( const K& key, const V& value ) {
            Trail trail( *this );
            if (!find<KT>( key, trail )) {
                auto page = leaf( trail );
                if (!page->entryFit()) {
                    grow<VT>( trail );
                    find<KT>( key, trail.clear() );
                    page = leaf( trail );
                }
                auto copyPage = updatePage<VT>( trail );
                if (trail.atSplit()) {
                    page->insert( 0, key, value, copyPage );
                } else {
                    page->insert( (trail.index() + 1), key, value, copyPage );
                }
                recoverPage( page->header, copyPage->header );
                return( true );
            }
            return( false );
        }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>&&S<VT>),bool> = true >
        bool insert( const B<KT>* key, PageSize keySize, const V& value ) {
            Trail trail( *this );
            if (!find<KT>( key, keySize, trail )) {
                auto page = leaf( trail );
                if (!page->entryFit( keySize )) {
                    grow<VT>( trail );
                    find<KT>( key, keySize, trail.clear() );
                    page = leaf( trail );
                }
                auto copyPage = updatePage<VT>( trail );
                if (trail.atSplit()) {
                    page->insert( 0, key, keySize, value, copyPage );
                } else {
                    page->insert( (trail.index() + 1), key, keySize, value, copyPage );
                }
                recoverPage( page->header, copyPage->header );
                return( true );
            }
            return( false );
        }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>&&S<VT>),bool> = true >
        bool insert( const std::pair< const B<KT>*, PageSize >& key, const V& value ) {
            return insert( key.first, key.second, value );
        }
        template< class KT = K, class VT = V, std::enable_if_t<(S<KT>&&A<VT>),bool> = true >
        bool insert( const K& key, const B<VT>* value, PageSize valueSize ) {
            Trail trail( *this );
            if (!find<KT>( key, trail )) {
                auto page = leaf( trail );
                if (!page->entryFit( valueSize )) {
                    grow<VT>( trail );
                    find<KT>( key, trail.clear() );
                    page = leaf( trail );
                }
                auto copyPage = updatePage<VT>( trail );
                if (trail.atSplit()) {
                    page->insert( 0, key, value, valueSize, copyPage );
                } else {
                    page->insert( (trail.index() + 1), key, value, valueSize, copyPage );
                }
                recoverPage( page->header, copyPage->header );
                return( true );
            }
            return( false );
        }
        template< class KT = K, class VT = V, std::enable_if_t<(S<KT>&&A<VT>),bool> = true >
        bool insert( const K& key, const std::pair< const B<VT>*, PageSize >& value ) {
            return insert( key, value.first, value.second );
        }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>&&A<VT>),bool> = true >
        bool insert( const B<KT>* key, PageSize keySize, const B<VT>* value, PageSize valueSize ) {
            Trail trail( *this );
            if (!find<KT>( key, keySize, trail )) {
                auto page = leaf( trail );
                if (!page->entryFit( keySize, valueSize )) {
                    grow<VT>( trail );
                    find<KT>( key, keySize, trail.clear() );
                    page = leaf( trail );
                }
                auto copyPage = updatePage<VT>( trail );
                if (trail.atSplit()) {
                    page->insert( 0, key, keySize, value, valueSize, copyPage );
                } else {
                    page->insert( (trail.index() + 1), key, keySize, value, valueSize, copyPage );
                }
                recoverPage( page->header, copyPage->header );
                return( true );
            }
            return( false );
        }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>&&A<VT>),bool> = true >
        bool insert( const std::pair< const B<KT>*, PageSize >& key, const std::pair< const B<VT>*, PageSize >& value ) {
            return insert( key.first, key.second, value.first, value.second );
        }

        // Replace a value for a key in the B-Tree.
        // Returns true if the value was replaced, false if the key was not present.
        // O(log(n)) complexity.
        template< class KT = K, class VT = V, std::enable_if_t<(S<KT>&&S<VT>),bool> = true >
        bool replace( const K& key, const V& value ) {
            Trail trail( *this );
            if (find<KT>( key, trail )) {
                auto page = leaf( trail );
                auto copyPage = updatePage<VT>( trail );
                if (trail.atSplit()) {
                    page->split( value, copyPage );
                } else {
                    page->replace( trail.index(), value, copyPage );
                }
                recoverPage( page->header, copyPage->header );
                return( true );
            }
            return( false );
        }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>&&S<VT>),bool> = true >
        bool replace( const B<KT>* key, PageSize keySize, const V& value ) {
            Trail trail( *this );
            if (find<KT>( key, keySize, trail )) {
                auto page = leaf( trail );
                auto copyPage = updatePage<VT>( trail );
                if (trail.atSplit()) {
                    page->split( value, copyPage );
                } else {
                    page->replace( trail.index(), value, copyPage );
                }
                recoverPage( page->header, copyPage->header );
                return( true );
            }
            return( false );
        }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>&&S<VT>),bool> = true >
        bool replace( const std::pair< const B<KT>*, PageSize >& key, const V& value ) {
            return replace( key.first, key.second, value );
        }
        template< class KT = K, class VT = V, std::enable_if_t<(S<KT>&&A<VT>),bool> = true >
        bool replace( const K& key, const B<VT>* value, PageSize valueSize ) {
            Trail trail( *this );
            if (find<KT>( key, trail )) {
                auto page = leaf( trail );
                // Only grow if value size actually increases...
                PageSize oldSize = ((trail.atSplit()) ? page->splitValueSize() : page->valueSize( trail.index() ));
                if ((oldSize < (valueSize * sizeof(B<VT>))) && (page->header.capacity < (page->filling() + ((valueSize * sizeof(B<VT>)) - oldSize)))) {
                    grow<VT>( trail );
                    find<KT>( key, trail.clear() );
                    page = leaf( trail );
                }
                auto copyPage = updatePage<VT>( trail );
                if (trail.atSplit()) {
                    page->split( value, valueSize, copyPage );
                } else {
                    page->replace( trail.index(), value, valueSize, copyPage );
                }
                recoverPage( page->header, copyPage->header );
                return( true );
            }
            return( false );
        }
        template< class KT = K, class VT = V, std::enable_if_t<(S<KT>&&A<VT>),bool> = true >
        bool replace( const K& key, const std::pair< const B<VT>*, PageSize >& value ) {
            return replace( key, value.first, value.second );
        }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>&&A<VT>),bool> = true >
        bool replace( const B<KT>* key, PageSize keySize, const B<VT>* value, PageSize valueSize ) {
            Trail trail( *this );
            if (find<KT>( key, keySize, trail )) {
                auto page = leaf( trail );
                // Only grow if value size actually increases...
                PageSize oldSize = ((trail.atSplit()) ? page->splitValueSize() : page->valueSize( trail.index() ));
                if ((oldSize < (valueSize * sizeof(B<VT>))) && (page->header.capacity < (page->filling() + ((valueSize * sizeof(B<VT>)) - oldSize)))) {
                    grow<VT>( trail );
                    find<KT>( key, keySize, trail.clear() );
                    page = leaf( trail );
                }
                auto copyPage = updatePage<VT>( trail );
                if (trail.atSplit()) {
                    page->split( value, valueSize, copyPage );
                } else {
                    page->replace( trail.index(), value, valueSize, copyPage );
                }
                recoverPage( page->header, copyPage->header );
                return( true );
            }
            return( false );
        }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>&&A<VT>),bool> = true >
        bool replace( const std::pair< const B<KT>*, PageSize >& key, const std::pair< const B<VT>*, PageSize >& value ) {
            return replace( key.first, key.second, value.first, value.second );
        }

        // Return value a particular key.
        // Throws an exception if key was not found.
        // O(log(n)) complexity.
        template< class KT = K, class VT = V, std::enable_if_t<(S<KT>&&S<VT>),bool> = true >
        const VT& retrieve( const KT& key) const {
            static const char* signature = "const V& Tree<K,V>::retrieve( const K& key) const";
            Trail trail( *this );
            if (!find<KT>( key, trail )) throw std::string( signature ) + " - Key not found";
            return value<VT>( trail );
        }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>&&S<VT>),bool> = true >
        const VT& retrieve( const B<KT>* key, PageSize keySize ) const {
            static const char* signature = "const V& Tree<K,V>::retrieve( const K* key, PageSize keySize ) const";
            Trail trail( *this );
            if (!find<KT>( key, keySize, trail )) throw std::string( signature ) + " - Key not found";
            return value<VT>( trail );
        }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>&&S<VT>),bool> = true >
        const VT& retrieve( const std::pair< const B<KT>*, PageSize >& key ) const {
            return retrieve( key.first, key.second );
        }
        template< class KT = K, class VT = V, std::enable_if_t<(S<KT>&&A<VT>),bool> = true >
        std::pair<const B<VT>*, PageSize> retrieve( const KT& key) const {
            static const char* signature = "std::pair<const V*, PageSize> Tree<K,V>::retrieve( const K& key) const";
            Trail trail( *this );
            if (!find<KT>( key, trail )) throw std::string( signature ) + " - Key not found";
            return value<VT>( trail );
        }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>&&A<VT>),bool> = true >
        std::pair<const B<VT>*, PageSize> retrieve( const B<KT>* key, PageSize keySize ) const {
            static const char* signature = "std::pair<const V*, PageSize> Tree<K,V>::retrieve( const K* key, PageSize keySize ) const";
            Trail trail( *this );
            if (!find<KT>( key, keySize, trail )) throw std::string( signature ) + " - Key not found";
            return value<VT>( trail );
        }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>&&A<VT>),bool> = true >
        std::pair<const B<VT>*, PageSize> retrieve( const std::pair< const B<KT>*, PageSize >& key ) const {
            return retrieve( key.first, key.second );
        }

        // Remove a key-value pair.
        // Returns true if key-value pair was removed, false if key-value pair was not found.
        // O(log(n)) complexity.
        template< class KT = K, std::enable_if_t<(S<KT>),bool> = true >
        bool remove( const KT& key ) {
            Trail trail( *this );
            if (!find<KT>( key, trail )) return false;
            removeAt<KT,V>( trail, key );
            return true;
        }
        template< class KT = K, std::enable_if_t<(A<KT>),bool> = true >
        bool remove( const B<KT>* key, PageSize keySize ) {
            Trail trail( *this );
            if (!find<KT>( key, keySize, trail )) return false;
            removeAt<KT,V>( trail, key, keySize );
            return true;
        }
        template< class KT = K, std::enable_if_t<(A<KT>),bool> = true >
        bool remove( const std::pair< const B<KT>*, PageSize >& key ) {
            return remove( key.first, key.second );
        }

        // Return iterator to begin of B-Tree
        template< class KT = K, class VT = V, std::enable_if_t<(S<KT>&&S<VT>),bool> = true >
        BTreeIterator<KT,VT, std::pair<const KT&, const VT&> > begin() const {
            auto iterator = new BTreeIterator<KT,VT, std::pair<const KT&, const VT&> >( *this );
            return iterator->begin();
        }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>&&S<VT>),bool> = true >
        BTreeIterator<KT,VT, std::pair<std::pair<const B<KT>*,PageSize>, const VT&> > begin() const {
            auto iterator = new BTreeIterator<KT,VT, std::pair<std::pair<const B<KT>*,PageSize>, const VT&> >( *this );
            return iterator->begin();
        }
        template< class KT = K, class VT = V, std::enable_if_t<(S<KT>&&A<VT>),bool> = true >
        BTreeIterator<KT,VT, std::pair<const KT&, std::pair<const B<VT>*,PageSize>> > begin() const {
            auto iterator = new BTreeIterator<KT,VT, std::pair<const KT&, std::pair<const B<VT>*,PageSize>> >( *this );
            return iterator->begin();
        }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>&&A<VT>),bool> = true >
        BTreeIterator<KT,VT, std::pair<std::pair<const B<KT>*,PageSize>, std::pair<const B<VT>*,PageSize>> > begin() const {
            auto iterator = new BTreeIterator<KT,VT, std::pair<std::pair<const B<KT>*,PageSize>, std::pair<const B<VT>*,PageSize>> >( *this );
            return iterator->begin();
        }
        // Return iterator to end of B-Tree
        template< class KT = K, class VT = V, std::enable_if_t<(S<KT>&&S<VT>),bool> = true >
        BTreeIterator<KT,VT, std::pair<const KT&, const VT&> > end() const {
            auto iterator = new BTreeIterator<KT,VT, std::pair<const KT&, const VT&> >( *this );
            return iterator->end();
        }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>&&S<VT>),bool> = true >
        BTreeIterator<KT,VT, std::pair<std::pair<const B<KT>*,PageSize>, const VT&> > end() const {
            auto iterator = new BTreeIterator<KT,VT, std::pair<std::pair<const B<KT>*,PageSize>, const VT&> >( *this );
            return iterator->end();
        }
        template< class KT = K, class VT = V, std::enable_if_t<(S<KT>&&A<VT>),bool> = true >
        BTreeIterator<KT,VT, std::pair<const KT&, std::pair<const B<VT>*,PageSize>> > end() const {
            auto iterator = new BTreeIterator<KT,VT, std::pair<const KT&, std::pair<const B<VT>*,PageSize>> >( *this );
            return iterator->end();
        }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>&&A<VT>),bool> = true >
        BTreeIterator<KT,VT, std::pair<std::pair<const B<KT>*,PageSize>, std::pair<const B<VT>*,PageSize>> > end() const {
            auto iterator = new BTreeIterator<KT,VT, std::pair<std::pair<const B<KT>*,PageSize>, std::pair<const B<VT>*,PageSize>> >( *this );
            return iterator->end();
        }
        // Return iterator to location of key in B-Tree.
        // If key cannot be found, returns end.
        template< class KT = K, class VT = V, std::enable_if_t<(S<KT>&&S<VT>),bool> = true >
        BTreeIterator<KT,VT, std::pair<const KT&, const VT&> > at( const KT& key ) const {
            Trail trail( *this );
            bool found = find<KT>( key, trail );
            auto iterator = new BTreeIterator<KT,VT, std::pair<const KT&, const VT&> >( *this );
            if (found) return iterator->at( trail );
            return iterator->end();
        }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>&&S<VT>),bool> = true >
        BTreeIterator<KT,VT, std::pair<std::pair<const B<KT>*,PageSize>, const VT&> > at( const B<KT>* key, PageSize keySize ) const {
            Trail trail( *this );
            bool found = find<KT>( key, keySize, trail );
            auto iterator = new BTreeIterator<KT,VT, std::pair<std::pair<const B<KT>*,PageSize>, const VT&> >( *this );
            if (found) return iterator->at( trail );
            return iterator->end();
        }
        template< class KT = K, class VT = V, std::enable_if_t<(S<KT>&&A<VT>),bool> = true >
        BTreeIterator<KT,VT, std::pair<const KT&, std::pair<const B<VT>*,PageSize>> > at( const KT& key ) const {
            Trail trail( *this );
            bool found = find<KT>( key, trail );
            auto iterator = new BTreeIterator<KT,VT, std::pair<const KT&, std::pair<const B<VT>*,PageSize>> >( *this );
            if (found) return iterator->at( trail );
            return iterator->end();
        }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>&&A<VT>),bool> = true >
        BTreeIterator<KT,VT, std::pair<std::pair<const B<KT>*,PageSize>, std::pair<const B<VT>*,PageSize>> > at( const B<KT>* key, PageSize keySize ) const {
            Trail trail( *this );
            bool found = find<KT>( key, keySize, trail );
            auto iterator = new BTreeIterator<KT,VT, std::pair<std::pair<const B<KT>*,PageSize>, std::pair<const B<VT>*,PageSize>> >( *this );
            if (found) return iterator->at( trail );
            return iterator->end();
        }

        // Empty the B-Tree
        // O(n) complexity.
        void clear() {
            freeAll( *root, true );
            root = &allocateLeaf()->header;
        }
        // Test if B-Tree is empty
        // O(1) complexity.
        bool empty() { return ((root->count == 0) && (root->split == 0)); }
        // Assign content of B-Tree
        // O(n log(n)) complexity.
        void assign( const Tree<K,V>& tree ) {
            clear();
            for ( auto src : tree ) insert( src.first, src.second );
        }
        // Return the number of key-value entries in the B-tree.
        // O(n) complexity.
        size_t size() const { return( pageCount( *root ) ); }
        // Test if particular key exists.
        // O(log(n)) complexity.
        template< class KT = K, class VT = V, std::enable_if_t<(S<KT>),bool> = true >
        bool exists( const KT& key) const {
            Trail trail( *this );
            return( find<KT>( key, trail ) );
        }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>),bool> = true >
        bool exists( const B<KT>* key, PageSize keySize ) const {
            Trail trail( *this );
            return( find<KT>( key, keySize, trail ) );
        }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>),bool> = true >
        bool exists( std::pair<const B<KT>*, PageSize>& key ) const {
            return exists( key.first, key.second );
        }
        // Return key comparison function used by this B-Tree
        template< class KT = K, std::enable_if_t<(S<KT>),bool> = true >
        ScalarKeyCompare keyCompare() const {
            return compare.scalar;
        }
        template< class KT = K, std::enable_if_t<(A<KT>),bool> = true >
        ArrayKeyCompare keyCompare() const {
            return compare.array;
        }

        // Collect PageLinks to all pages in use by the B-Tree.
        std::vector<PageLink>* collectPages() const {
            auto links = new std::vector<PageLink>();
            collectPage( *links, root->page  );
            return links;
        }

    private:

        // Allocate a new Page from the page pool and (optionally) mark it as modified.
        template< class VT >
        inline Page<B<K>,B<VT>,A<K>,A<VT>>* allocatePage( const PageDepth depth, bool modify = true ) const {
            Page<B<K>,B<VT>,A<K>,A<VT>>* page = pool.page<B<K>,B<VT>,A<K>,A<VT>>( depth );
            if (modify) pool.modify( page->header );
            return( page );
        }
        template< class VT >
        inline Page<B<K>,B<VT>,A<K>,A<VT>>* allocatePage( const PageHeader& header, bool modify = true ) const {
            return( allocatePage<VT>( header.depth, modify ) );
        }
    
        inline Page<B<K>,B<V>,A<K>,A<V>>* allocateLeaf( bool modify = true ) const {
            return allocatePage<V>( 0, modify );
        }
        inline Page<B<K>,PageLink,A<K>,false>* allocateNode( const PageDepth depth, bool modify = true ) const {
            return( allocatePage<PageLink>( depth, modify ) );
        }

        // Convert a PageHeader pointer to a Page instance.
        template< class VT >
        inline Page<B<K>,B<VT>,A<K>,A<VT>>* page( const PageHeader* header ) const {
            return( pool.page<B<K>,B<VT>,A<K>,A<VT>>( header ) );
        }
        // Convert a Trail to a Page instance.
        template< class VT >
        inline Page<B<K>,B<VT>,A<K>,A<VT>>* page( const Trail& trail, PageDepth offset = 0 ) const {
            return( pool.page<B<K>,B<VT>,A<K>,A<VT>>( trail.header( offset ) ) );
        }
        // Access Leaf Page addressed by Trail.
        // Returns null pointer if Trail does not address a Leaf Page.
        inline Page<B<K>,B<V>,A<K>,A<V>>* leaf( const Trail& trail ) const {
            const PageHeader* header = trail.header();
            if (header->depth != 0) return( nullptr );
            return( pool.page<B<K>,B<V>,A<K>,A<V>>( header ) );
        }
        // Access Node Page addressed by Trail (at optional depth offset).
        // Returns null pointer if Trail does not address a Node Page.
        inline Page<B<K>,PageLink,A<K>,false>* node( const Trail& trail, PageDepth offset = 0 ) const {
            const PageHeader* header = trail.header( offset );
            if (header->depth == 0) return( nullptr );
            return( pool.page<B<K>,PageLink,A<K>,false>( header ) );
        }

        // Recursively copy-on-update nodes in trail up to root.
        // Stop when a modified node or root is encountered.
        void updateNodeTrail( Trail& trail, const PageHeader& header ) {
            Trail::Position position = trail.position();
            PageIndex index = trail.index();
            trail.pop();
            if (trail.depth() == 0) {
                root = &( const_cast<PageHeader&>(header) );
            } else {
                auto node = page<PageLink>( trail.header() );
                if (node->header.modified) {
                    if (trail.atSplit()) {
                        node->split( header.page );
                    } else {
                        node->replace( trail.index(), header.page );
                    }
                } else {
                    auto copy = allocateNode( node->header.depth );
                    if (trail.atSplit()) {
                        node->split( header.page, copy );
                    } else {
                        node->replace( trail.index(), header.page, copy );
                    }
                    recoverPage( node->header, copy->header );
                    updateNodeTrail( trail, copy->header );
                }
            }
            trail.push( &header, position, index );
        }

        // An update to the BTree at trail is to be performed.
        // This can be an insertion, replacement or removal of a key-value.
        // Enforce update semantics being one of:
        //   InPlace -> no copy-on-update
        //   MemoryTransaction -> copy-on-update without memory reuse
        //   PersistentTransaction -> copy-on-update with memory reuse and back-up storage
        // Memory reuse (through recover) to be called after actual update is perfomed.
        // Does nothing if page is already modified; i.e., accessing copy of an earlier update.
        // The functions updatePage and recoverPage are used to enclose code that updates
        // the content of a page as follows:
        //   Page<K,V,KA,VT>* page = ...
        //   Page<K,V,KA,VT>* copy = updatePage<V>( trail );
        //      ... code that updates page such as
        //      page->insert( index, key, value, copy );
        //      ...
        //   recoverPage( page->header, copy->header );
        // All page modification functions have an optional argument for copy-on-update behavior.
        template< class VT >
        Page<B<K>,B<VT>,A<K>,A<VT>>* updatePage( Trail& trail ) {
            auto page = this->page<VT>( trail.header() );
            if (mode == InPlace) pool.modify( page->header );
            if (page->header.modified) return page;
            auto copyPage = allocatePage<VT>( page->header );
            updateNodeTrail( trail, copyPage->header );
            return copyPage;
        }
        // Recover (persistent) src page if it differs from dst page;
        // i.e., dst is the copy-on-update version of src.
        inline void recoverPage( const PageHeader& src, const PageHeader& dst ) {
            if (src.page != dst.page) pool.recover( src, true );
        }

        // Find a particular key.
        // Returns true if the key was found, false otherwise.
        // As a result, the trail will index a (found) key in a leaf page.
        // If key is associated with a split value, trail index will be 0 and compare negative.
        // For split values, a found key will reside in a node page eariler in trail.
        // If key is not found the indexed key is the largest key less than the requested key.
        // The optional depth indicates to which BTree depth to find. Defaults to 0, finding to leaf.
        // Find a scalar key.
        template< class KT, std::enable_if_t<(S<KT>),bool> = true >
        bool find( const KT& key, Trail& trail, PageDepth to = 0 ) const {
            bool found = false;
            bool keyFound;
            auto leafPage = leaf( trail );
            if (leafPage) {
                keyFound = LeafCompareScalar<KT,B<V>,A<V>>( leafPage, key, compare.scalar ).position( trail );
                found |= keyFound;
            } else {
                auto nodePage = node( trail );
                keyFound = NodeCompareScalar<KT>( nodePage, key, compare.scalar ).position( trail );
                if (nodePage->header.depth == to) return found;
                if (trail.onIndex()) {
                    PageLink link = nodePage->value( trail.index() );
                    find<KT>( key, trail.push( pool.reference( link ) ), to );
                    found = true;
                } else if (trail.atSplit() || ((nodePage->header.count == 0) && nodePage->splitDefined())) {
                    PageLink link = nodePage->split();
                    keyFound = find<KT>( key, trail.push( pool.reference( link ) ), to );
                    found |= keyFound;
                } else if (0 <= trail.index()) {
                    PageLink link = nodePage->value( trail.index() );
                    keyFound = find<KT>( key, trail.push( pool.reference( link ) ), to );
                    found |= keyFound;
                }
            }
            return found;
        }
        // Find an array key.
        template< class KT, std::enable_if_t<(A<KT>),bool> = true >
        bool find( const B<KT>* key, PageSize keySize, Trail& trail, PageDepth to = 0 ) const {
            bool found = false;
            bool keyFound;
            auto leafPage = leaf( trail );
            if (leafPage) {
                keyFound = LeafCompareArray<B<KT>,B<V>,A<V>>( leafPage, key, keySize, compare.array ).position( trail );
                found |= keyFound;
            } else {
                auto nodePage = node( trail );
                keyFound = NodeCompareArray<B<KT>>( nodePage, key, keySize, compare.array ).position( trail );
                if (nodePage->header.depth == to) return found;
                if (trail.onIndex()) {
                    PageLink link = nodePage->value( trail.index() );
                    find<KT>( key, keySize, trail.push( pool.reference( link ) ), to );
                    found = true;
                } else if (trail.atSplit() || ((nodePage->header.count == 0) && nodePage->splitDefined())) {
                    PageLink link = nodePage->split();
                    keyFound = find<KT>( key, keySize, trail.push( pool.reference( link ) ), to );
                    found |= keyFound;
                } else if (0 <= trail.index()) {
                    PageLink link = nodePage->value( trail.index() );
                    keyFound = find<KT>( key, keySize, trail.push( pool.reference( link ) ), to );
                    found |= keyFound;
                }
            }
            return found;
        }
        template< class KT, class VT, std::enable_if_t<(S<KT>),bool> = true >
        bool find( const Page<B<KT>,B<VT>,false,A<VT>>* page, Trail& trail ) const {
            return find<KT>( page->key( 0 ), trail, page->header.depth );
        }
        template< class KT, class VT, std::enable_if_t<(A<KT>),bool> = true >
        bool find( const Page<B<KT>,B<VT>,true,A<VT>>* page, Trail& trail ) const {
            return find<KT>( page->key( 0 ), page->keySize( 0 ), trail, page->header.depth );
        }
        // Insert a key-link in a Node Page.
        // Insert scalar key-link
        template< class KT, std::enable_if_t<(S<KT>),bool> = true >
        void nodeInsert( Trail& trail, const K& key, PageLink link ) {
            static const char* signature( "void Tree<K,V>::nodeInsert( Trail& trail, const K& key, PageLink link )" );
            if (trail.depth() == 0) {
                if (Trail::MaxHeight < (root->depth + 1)) throw std::string( signature ) + " - Max BTree heigth exceeded";
                PageHeader* oldRoot = root;
                auto node = allocateNode( root->depth + 1 );
                root = &node->header;
                node->split( oldRoot->page );
                node->insert( 0, key, link );
                trail.clear();
            } else {
                auto parent = node( trail );
                if (!parent->entryFit()) {
                    grow<PageLink>( trail );
                    find<KT>( key, trail.clear(), parent->header.depth );
                    parent = node( trail );
                }
                auto copy = updatePage<PageLink>( trail );
                if ((parent->header.count == 0) or (trail.atSplit())) {
                    parent->insert( 0, key, link, copy );
                } else {
                    parent->insert( (trail.index() + 1), key, link, copy );
                }
                recoverPage( parent->header, copy->header );
            }
        }
        // Insert array key-link
        template< class KT, std::enable_if_t<(A<KT>),bool> = true >
        void nodeInsert( Trail& trail, const B<KT>* key, PageSize keySize, PageLink link ) {
            static const char* signature( "void Tree<K,V>::nodeInsert( Trail& trail, const K* key, PageSize keySize, PageLink link )" );
            if (trail.depth() == 0) {
                if (Trail::MaxHeight < (root->depth + 1)) throw std::string( signature ) + " - Max BTree heigth exceeded";
                PageHeader* oldRoot = root;
                auto parent = allocateNode( root->depth + 1 );
                root = &parent->header;
                parent->split( oldRoot->page );
                parent->insert( 0, key, keySize, link );
                trail.clear();
            } else {
                auto parent = node( trail );
                if (!parent->entryFit( keySize )) {
                    grow<PageLink>( trail );
                    find<KT>( key, keySize, trail.clear(), parent->header.depth );
                    parent = node( trail );
                }
                auto copy = updatePage<PageLink>( trail );
                if ((parent->header.count == 0) or (trail.atSplit())) {
                    parent->insert( 0, key, keySize, link, copy );
                } else {
                    parent->insert( (trail.index() + 1), key, keySize, link, copy );
                }
                recoverPage( parent->header, copy->header );
            }
        }
        template< class VT, std::enable_if_t<(S<VT>),bool> = true >
        inline void setSplit( Page<B<K>,VT,A<K>,false>& dst, const Page<B<K>,VT,A<K>,false>& src, PageIndex index ) const {
            dst.split( src.value( index ) );
        }
        template< class VT, std::enable_if_t<(A<VT>),bool> = true >
        inline void setSplit( Page<B<K>,B<VT>,A<K>,true>& dst, const Page<B<K>,B<VT>,A<K>,true>& src, PageIndex index ) const {
            dst.split( src.value( index ), src.valueSize( index ) );
        }
        template< class KT, class VT, std::enable_if_t<S<KT>,bool> = true >
        inline void insertSplitKey( Trail& trail, const Page<KT,B<VT>,false,A<VT>>& src, PageIndex index, PageLink link ) {
            nodeInsert<KT>( trail, src.key( index ), link );
        }
        template< class KT, class VT, std::enable_if_t<A<KT>,bool> = true >
        inline void insertSplitKey( Trail& trail, const Page<B<KT>,B<VT>,true,A<VT>>& src, PageIndex index, PageLink link ) {
            nodeInsert<KT>( trail, src.key( index ), src.keySize( index ), link );
        }
        // Find index that splits page into *this of (nearly) equal filling.
        // This is not necessarily half-way in entries for *this with variable size keys and/or values.
        template< class KT, class VT >
        PageIndex optimalCut( Page<B<KT>,B<VT>,A<KT>,A<VT>>* node ) {
            PageIndex cut = (node->header.count / 2);
            PageSize fill = node->filling();
            PageSize leftFill = node->filling( cut );
            PageSize rightFill = (fill - leftFill);
            PageSize balance = ((leftFill < rightFill) ? (rightFill - leftFill) : (leftFill - rightFill));
            if (leftFill < rightFill) {
                while ((leftFill < rightFill) && ((cut + 1) < node->header.count)) {
                    PageIndex probe = (cut + 1);
                    leftFill = node->filling( probe );
                    rightFill = (fill - leftFill);
                    PageSize newBalance = ((leftFill < rightFill) ? (rightFill - leftFill) : (leftFill - rightFill));
                    if (balance < newBalance) return( cut );
                    balance = newBalance;
                    cut = probe;
                }
            } else {
                while ((rightFill < leftFill) && (0 < (cut - 1))) {
                    PageIndex probe = (cut - 1);
                    leftFill = node->filling( probe );
                    rightFill = (fill - leftFill);
                    PageSize newBalance = ((leftFill < rightFill) ? (rightFill - leftFill) : (leftFill - rightFill));
                    if (balance < newBalance) return( cut );
                    balance = newBalance;
                    cut = probe;
                }
            }
            return( cut );
        }
        // Create room in Page for insertion by adding a Page to the right
        // This will result in adding a key to the parent page.
        // Content is is shifted to the newly created Page, making room in both Pages.
        // This can fail if maximum tree depth is exceeded.
        template< class VT >
        void grow( Trail& trail ) {
            auto node = page<VT>( trail );
            auto copyNode = updatePage<VT>( trail );
            auto right = allocatePage<VT>( node->header );
            node->shiftRight( *right, optimalCut<K,VT>( node ), copyNode );
            PageIndex splitKeyIndex = (copyNode->header.count - 1);
            setSplit<VT>( *right, *copyNode, splitKeyIndex );
            insertSplitKey<K,VT>( trail.pop(), *copyNode, splitKeyIndex, right->header.page );
            copyNode->remove( splitKeyIndex );
            recoverPage( node->header, copyNode->header );
        }

        // Remove current split and set split in Page to next key-value (if any)
        // Add new split key to appropriate node
        template< class KT = K, class VT = V, std::enable_if_t<(S<KT>),bool> = true >
        void nextSplit( Trail& trail, const K& key ) {
            // Set split for fixed size keys
            auto page = this->page<VT>( trail );
            if (0 < page->header.count) {
                // Shift Page value 0 into split and update corresponding Node key to Page key 0
                // Shift into new page for convenience (minimize memory rearrangement)
                auto newPage = allocatePage<VT>( page->header );
                setSplit<VT>( *newPage, *page, 0 );
                if (1 < page->header.count) page->shiftRight( *newPage, 1 );
                auto parent = node( trail.pop() );
                auto copyParent = updatePage<PageLink>( trail );
                if (trail.atSplit()) {
                    parent->split( newPage->header.page, copyParent );
                    PageDepth offset = trail.match();
                    // Ancestor page is already copied due to earlier updatePage...
                    auto ancestor = node( trail, offset );
                    PageIndex index = trail.index( offset );
                    ancestor->replace( index, page->key(0), ancestor->value(index) );
                } else {
                    parent->replace( trail.index(), page->key(0), newPage->header.page, copyParent );
                }
                recoverPage( parent->header, copyParent->header );
                trail.pushSplit( &newPage->header );
                conditionalMerge( trail );
            } else {
                // Page is empty after removing split...
                removeAt<KT,PageLink>( trail.pop(), key );
            }
            pool.recover( page->header, true );
        }
        // Remove current split and set split in Page to next key-value (if any)
        // Add new split key to appropriate node
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>),bool> = true >
        void nextSplit( Trail& trail, const B<KT>* key, PageSize keySize ) {
            // Set split for variable size keys
            auto page = this->page<VT>( trail );
            if (0 < page->header.count) {
                // Shift Page key 0 into split and update corresponding Node key to match
                // Shift into new page for convenience (minimize memory rearrangement)
                auto newPage = allocatePage<VT>( page->header );
                setSplit<VT>( *newPage, *page, 0 );
                if (1 < page->header.count) page->shiftRight( *newPage, 1 );
                auto ancestor = node( trail, trail.match() );
                while (!ancestor->entryFit( page->keySize(0) )) {
                    grow<PageLink>( trail.popToMatch() );
                    find<KT>( key, keySize, trail.clear(), page->header.depth );
                    ancestor = node( trail, trail.match() );
                }
                auto parent = node( trail.pop() );
                auto copyParent = updatePage<PageLink>( trail );
                if (trail.atSplit()) {
                    parent->split( newPage->header.page, copyParent );
                    PageDepth offset = trail.match();
                    auto ancestor = node( trail, offset );
                    PageIndex index = trail.index( offset );
                    ancestor->replace( index, page->key(0), page->keySize(0), ancestor->value(index) );
                } else {
                    parent->replace( trail.index(), page->key(0), page->keySize(0), newPage->header.page, copyParent );
                }
                recoverPage( parent->header, copyParent->header );
                trail.pushSplit( &newPage->header );
                conditionalMerge( trail );
            } else {
                removeAt<KT,PageLink>( trail.pop(), key, keySize );
            }
            pool.recover( page->header, true );
        }

        // Remove entry at trail.
        // Remove scalar key entry at trail.
        template< class KT = K, class VT = V, std::enable_if_t<(S<KT>),bool> = true >
        void removeAt( Trail& trail, const KT& key ) {
            auto page = this->page<VT>( trail );
            if (trail.atIndex()) {
                // Removing a key-value pair in this page.
                PageIndex index = trail.index();
                auto copyPage = updatePage<VT>( trail );
                page->remove( index, copyPage );
                trail.deletedIndex();
                recoverPage( page->header, copyPage->header );
                conditionalMerge( trail );
            } else {
                // Removing key to split value in this page.
                nextSplit<KT,VT>( trail, key );
                pruneRoot();
            }
        }
        // Remove array key entry at trail.
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>),bool> = true >
        void removeAt( Trail& trail, const B<KT>* key, PageSize keySize ) {
            auto page = this->page<VT>( trail );
            if (trail.atIndex()) {
                // Removing a key-value pair in this page.
                PageIndex index = trail.index();
                auto copyPage = updatePage<VT>( trail );
                page->remove( index, copyPage );
                trail.deletedIndex();
                recoverPage( page->header, copyPage->header );
                conditionalMerge( trail );
            } else {
                // Removing key to split value in this page.
                nextSplit<KT,VT>( trail, key, keySize );
                pruneRoot();
            }
        }
        // Return scalar value at trail to leaf
        template< class VT, std::enable_if_t<(S<VT>),bool> = true >
        const V& value( const Trail& trail ) const {
            const auto page = leaf( trail );
            if (trail.onIndex()) return page->value( trail.index() );
            return page->split();
        }
        // Return array value at trail to leaf
        template< class VT, std::enable_if_t<(A<VT>),bool> = true >
        std::pair<const B<VT>*, PageSize> value( const Trail& trail ) const {
            const auto page = leaf( trail );
            if (trail.onIndex()) return{ page->value( trail.index() ), page->valueSize( trail.index() ) };
            return{ page->split(), page->splitSize() };
        }
        // Append split with value in src and key in ancestor at index to src.
        // Append scalar value with scalar key
        template< class VT, std::enable_if_t<(S<K>&&S<VT>),bool> = true >
        inline void appendSplit( Page<K,VT,false,false>& dst, const Page<K,PageLink,false,false>& ancestor, PageIndex index, const Page<K,VT,false,false>& src, Page<K,VT,false,false>* copy = nullptr ) {
            dst.insert( dst.header.count, ancestor.key( index ), src.split(), copy );
        }
        // Append array value with scalar key
        template< class VT, std::enable_if_t<(S<K>&&A<VT>),bool> = true >
        inline void appendSplit( Page<K,B<VT>,false,true>& dst, const Page<K,PageLink,false,false>& ancestor, PageIndex index, const Page<K,B<VT>,false,true>& src, Page<K,B<VT>,false,true>* copy = nullptr ) {
            dst.insert( dst.header.count, ancestor.key( index ), src.split(), src.splitSize(), copy );
        }
        // Append scalar value with array key
        template< class VT, std::enable_if_t<(A<K>&&S<VT>),bool> = true >
        inline void appendSplit( Page<B<K>,VT,true,false>& dst, const Page<B<K>,PageLink,true,false>& ancestor, PageIndex index, const Page<B<K>,VT,true,false>& src, Page<B<K>,VT,true,false>* copy = nullptr ) {
            dst.insert( dst.header.count, ancestor.key( index ), ancestor.keySize( index ), src.split(), copy );
        }
        // Append array value with array key
        template< class VT, std::enable_if_t<(A<K>&&A<VT>),bool> = true >
        inline void appendSplit( Page<B<K>,B<VT>,true,true>& dst, const Page<B<K>,PageLink,true,false>& ancestor, PageIndex index, const Page<B<K>,B<VT>,true,true>& src, Page<B<K>,B<VT>,true,true>* copy = nullptr ) {
            dst.insert( dst.header.count, ancestor.key( index ), ancestor.keySize( index ), src.split(), src.splitSize(), copy );
        }

        template< class KT, std::enable_if_t<(S<KT>),bool> = true >
        inline void locateSplit( Trail& trail, const Page<KT,PageLink,false,false>& node ) const {
            find<KT>( node.key( 0 ), trail );
        }
        template< class KT, std::enable_if_t<(A<KT>),bool> = true >
        inline void locateSplit( Trail& trail, const Page<B<KT>,PageLink,true,false>& node ) const {
            find<KT>( node.key( 0 ), node.keySize( 0 ), trail );
        }
        // Access previous page at trail (referencing node page)
        // Returns null if there is no previous page
        // Trail must be positioned on a node Page.
        template< class VT >
        Page<B<K>,B<VT>,A<K>,A<VT>>* previousPage( Trail& trail ) {
            if ( trail.previous<B<K>,A<K>>() ) {
                const PageHeader* header = trail.pageHeader<K,A<K>>();
                if (0 < header->count) {
                    trail.pushIndex( header, (header->count - 1) );
                } else {
                    trail.pushSplit( header );
                }
                return page<VT>( header );
            }
            return nullptr;
        }
        // Access next page at trail (referencing node page).
        // Returns null if there is no next page
        // Trail must be positioned on a node Page.
        template< class VT >
        Page<B<K>,B<VT>,A<K>,A<VT>>* nextPage( Trail& trail ) {
            static const char* signature( "Page<K,V,KA,VA>* Tree<K,V>::nextPage( Trail& trail )" );
            if ( trail.next<B<K>,A<K>>() ) {
                const PageHeader* header = trail.pageHeader<K,A<K>>();
                if (0 < header->split) {
                    trail.pushSplit( header );
                } else {
                    if (header->count == 0) throw std::string( signature ) + " - Navigating to empty page";
                    trail.pushIndex( header , 0 );
                }
                return page<VT>( header );
            }
            return nullptr;
        }
        // Append content of src Page to dst Page, adjusting split key and
        // removing node reference to (appended) src page.
        // Trail arguments point to src and dst respectively.
        // Merging is always from right to left (using Page::shiftLeft)
        template< class KT, class VT>
        void mergePage( Trail& dstTrail, Trail& srcTrail ) {
            const auto src = page<VT>( srcTrail );
            auto dst = page<VT>( dstTrail );
            auto dstCopy = updatePage<VT>( dstTrail );
            srcTrail.pop();
            dstTrail.pop();
            PageDepth offset = srcTrail.match();
            PageIndex index = srcTrail.index( offset );
            Page<B<KT>,PageLink,A<KT>,false>* ancestor = page<PageLink>( srcTrail.header( offset ) );
            PageLink ancestorLink = ancestor->value( index );
            // Page being merged must have a split value as it cannot be leftmost leaf.
            appendSplit<VT>( *dst, *ancestor, index, *src, dstCopy );
            src->shiftLeft( *dstCopy, src->header.count );
            recoverPage( dst->header, dstCopy->header );
            pool.recover( src->header, true );
            // Recursively merge ancestor nodes where possible
            // A non-empty ancestor node must exist as we just merged two pages
            pruneBranch( srcTrail );
            // Remove link to recently merged page
            auto parent = node( srcTrail );
            if (parent == ancestor) {
                ancestor->remove( index );
                srcTrail.deletedIndex();
            } else {
                // Replace split key in ancestor with next largest key in parent
                auto keyPark = allocateNode( parent->header.depth, false );
                appendSplit<PageLink>( *keyPark, *parent, 0, *parent );
                parent->split( parent->value( 0 ) );
                parent->remove( 0 );
                ancestor->remove( index );
                srcTrail.popToMatch().deletedIndex();
                insertSplitKey<KT,PageLink>( srcTrail, *keyPark, 0, ancestorLink );
                pool.free( keyPark->header );
            }
            pruneRoot();
            // Merged page (dstCopy) has at least one key-value entry, find it using key( 0 )
            find<KT,VT>( dstCopy, srcTrail.clear() );
            if (1 < srcTrail.depth()) {
                parent = node( srcTrail.pop() );
                if (parent->filling() < (LowPageThreshold * parent->header.capacity) ) merge<KT,PageLink>( srcTrail );
            }
        }
        // Prune tree of degenerate nodes up to non-empty node
        // A degenerate node being a node containing only a split link
        void pruneBranch( Trail& trail ) {
            auto node = page<PageLink>( trail );
            while (node->size() == 0) {
                pool.recover( node->header, true );
                node = page<PageLink>( trail.pop() );
            }
        }
        // Set new root if current root is degenerate.
        void pruneRoot() {
            while ((root->count == 0) && (0 < root->depth)) {
                auto rootPage = page<PageLink>( this->root );
                root = pool.reference( rootPage->split() );
                pool.recover( rootPage->header, true );
            }
        }
        template< class KT, std::enable_if_t<(S<KT>),bool> = true >
        PageSize splitKeySize( const Trail& trail ) const {
            PageDepth offset = trail.match();
            PageIndex index = trail.index( offset );
            const auto ancestor = node( trail, offset );
            if (index < ancestor->header.count) return( sizeof(KT) );
            return( 0 );
        }
        template< class KT, std::enable_if_t<(A<KT>),bool> = true >
        PageSize splitKeySize( const Trail& trail ) const {
            PageDepth offset = trail.match();
            PageIndex index = trail.index( offset );
            const auto ancestor = node( trail, offset );
            if (index < ancestor->header.count) return( sizeof(B<KT>) * ancestor->keySize( index ) );
            return( 0 );
        }
        // Merge neighbouring Pages, if any
        // This may result recursively merging Pages higher up in BTree, eventually reducing BTree height.
        template< class KT, class VT >
        void merge( Trail& trail ) {
            const PageHeader* header = trail.header();
            if (header != root) {
                Trail pageTrail( trail );
                trail.pop();
                const auto page = this->page<VT>( header );
                Trail leftTrail( trail );
                const auto leftPage = previousPage<VT>( leftTrail );
                PageSize leftFill = (2 * page->header.capacity);
                if (leftPage) {
                    leftFill = leftPage->filling();
                    leftFill += page->indexedFilling( page->size() );
                    PageSize leftSplitKeySize = splitKeySize<KT>( trail );
                    if (0 < leftSplitKeySize) leftFill += (leftSplitKeySize + page->splitValueSize());
                }
                Trail rightTrail( trail );
                const auto rightPage = nextPage<VT>( rightTrail );
                PageSize rightFill = (2 * page->header.capacity);
                if (rightPage) {
                    rightFill = rightPage->indexedFilling( rightPage->size() );
                    PageSize rightSplitKeySize = splitKeySize<KT>( rightTrail );
                    if (0 < rightSplitKeySize) rightFill += (rightSplitKeySize + rightPage->splitValueSize());
                    rightFill += page->filling();
                }
                PageSize threshold = (HighPageThreshold * page->header.capacity);
                if ((leftFill < rightFill) && (leftFill < threshold)) {
                    mergePage<KT,VT>( leftTrail, pageTrail ); // Shift content of this page to left page
                } else if (rightFill < threshold) {
                    mergePage<KT,VT>( pageTrail, rightTrail ); // Shift content of right page to this page
                }
            }
        }
        // Merge either Leaf or Node *this depending on page level at top of trail.
        void conditionalMerge( Trail& trail ) {
            const PageHeader* header = trail.header();
            if (header->depth == 0) {
                auto page = leaf( trail );
                if (page->filling() < (LowPageThreshold * page->header.capacity) ) merge<K,V>( trail );
            } else {
                auto page = node( trail );
                if (page->filling() < (LowPageThreshold * page->header.capacity) ) merge<K,PageLink>( trail );
            }
        }

        // Free a page and all pages that it references; i.e., free an entire (sub-) tree.
        void freeAll( const PageHeader& page, bool recover ) {
            if (page.free != 1) {
                if (0 < page.depth) {
                    Page<B<K>,PageLink,A<K>,false>* node = this->page<PageLink>( &page );
                    if (node->splitDefined()) freeAll( pool.access( node->split() ), recover );
                    for (PageIndex i = 0; i < node->size(); i++) freeAll( pool.access( node->value( i ) ), recover );
                }
                if (recover) pool.recover( page, true );
            }
        }

        // Recursively count the number of key-value entries in a page.
        size_t pageCount( const PageHeader& header ) const {
            static const char* signature( "size_t Tree<K,V>::pageCount( const PageHeader& header ) const" );
            if (header.free == 1)  throw std::string(signature) + " - Accessing freed page.";
            size_t count = header.count;
            if (0 < header.depth) {
                const Page<B<K>,PageLink,A<K>,false>& node = *page<PageLink>( &header );
                if (node.splitDefined()) count += pageCount( pool.access( node.split() ) );
                for (PageIndex i = 0; i < header.count; i++) count += pageCount( pool.access( node.value( i )) );
            }
            return( count );
        }

        void collectPage( std::vector<PageLink>& links, PageLink link ) const {
            links.push_back( link );
            const PageHeader& header = pool.access( link );
            if ( 0 < header.depth ) {
                const Page<B<K>,PageLink,A<K>,false>& node = *pool.page<B<K>,PageLink,A<K>,false>( link );
                if (node.splitDefined()) collectPage( links, node.split() );
                for (PageIndex i = 0; i < header.count; ++i) collectPage( links, node.value( i ) );
            }
        }

        void streamPage( std::ostream & o, PageLink link, PageDepth depth ) const {
            if (pool.valid( link )) {
                const PageHeader& header = pool.access( link );
                if (header.depth == 0) {
                    auto leaf = pool.page<B<K>,B<V>,A<K>,A<V>>( &header );
                    if (header.depth == depth) o << *leaf;
                } else {
                    auto node = pool.page<B<K>,PageLink,A<K>,false>( &header );
                    if (header.depth == depth) o << *node;
                    if (node->splitDefined()) {
                        PageLink link = node->split();
                        if (pool.valid( link )) {
                            streamPage( o, link, depth );
                        } else {
                            o << "Invalid link " << link << " at split!\n";
                        }

                    }
                    for (PageIndex i = 0; i < node->size(); i++) {
                        PageLink link = node->value( i );
                        if (pool.valid( link )) {
                            streamPage( o, link, depth );
                        } else {
                            o << "Invalid link " << link << " at index " << i << "!\n";
                        }
                    }
                }
            } else o << "Invalid page link " << link << "\n";
        }

    public:

        template< class KT = K, class VT = V >
        Tree(
            std::enable_if_t<(S<KT>),PagePool>& pagePool,
            int(*compareKey)( const B<KT>&, const B<KT>& ),
            UpdateMode updateMode,
            PageHeader* pageRoot
        ) : TreeBase( pagePool, pageRoot, updateMode ) {
            if (root == nullptr) root = &allocateLeaf()->header;
            compare.scalar = compareKey;
        }
        template< class KT = K, class VT = V >
        Tree(
            std::enable_if_t<(A<KT>),PagePool>& pagePool,
            int(*compareKey)( const B<KT>*, PageSize, const B<KT>*, PageSize ),
            UpdateMode updateMode,
            PageHeader* pageRoot
        ) : TreeBase( pagePool, pageRoot, updateMode ) {
            if (root == nullptr) root = &allocateLeaf()->header;
            compare.array = compareKey;
        }

    protected:

        // Recover B-Tree to a root defined by a PageLink. 
        void recoverTree( PageLink link ) {
            if (link.null()) {
                // No previous B-Tree state, recover to empty BTree
                freeAll( *root, true );
                root = &allocateLeaf()->header;
            } else {
                root = pool.reference( link );
            }
        }
        
        void stream( std::ostream & o ) const {
            o << "Root\n";
            for (int d = root->depth; 0 <= d; d--) streamPage( o, root->page, d );
        }

    }; // class Tree

    template< class K, class V >
    inline std::ostream & operator<<( std::ostream & o, const Tree<K,V>& tree ) { tree.stream( o ); return o; }

}; // namespace BTree

#include "BTreeIterator.h"

#endif // BTREE_TREE_H