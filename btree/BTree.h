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

// ToDo: Provide function to compact B-Tree to contiguous pages (reducing memory usage and persistent file size)
// ToDo: Provide merge function on entire B-Trees
// ToDo: Avoid additional lookUp when growing (gain is marginal for page capacity large relative to key-value size)
// ToDo: Try to reduce number of unsuccessful merge attempts (adapt thresholds dynamically?)
// ToDo: Improve filling by distributing page content left and right during merge.
// ToDo: Avoid growing by distributing page content left and/or right. Will probably also improve filling.

namespace BTree {

    template< class K, class V, class T > class TreeIterator;

    static const float LowPageThreshold = 0.4f;
    static const float HighPageThreshold = 0.9f;

    // B-Tree with template arguments for key (K) and value (V).
    //
    // A B-Tree maps keys to values.
    //
    // For example:
    //   Tree< int, float > - maps integer keys to float values.
    // Either or both arguments may be (variable length) arrays.
    //   Tree< char[], float > - maps C-string keys to float values.
    //   Tree< int, char[] > - maps integer keys to C-string values.
    //   Tree< char[], uint16_t[] > - maps C-string keys to arrays of 16-bit unsigned integers.
    //
    // Performance cost of an operation on the B-Tree is typically O(logN(n)*log(N)) where N is the
    // average number of entries stored in a Page and n is the total number of entries.
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
        friend class TreeIterator;
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
        // O(logN(n)*log(N)) complexity.
        template< class KT = K, class VT = V, std::enable_if_t<(S<KT>&&S<VT>),bool> = true >
        bool insert( const K& key, const V& value ) {
            Trail trail( *this );
            if (!lookUp<KT>( key, trail )) {
                if (stats) stats->insertions += 1;
                auto page = leaf( trail );
                if (!page->entryFit()) {
                    page = allocate<KT,VT>( trail, key, page->entryFilling() );
                }
                auto copyPage = updatePage<VT>( trail );
                if (trail.atSplit()) {
                    page->insert( 0, key, value, copyPage );
                } else {
                    page->insert( (trail.index() + 1), key, value, copyPage );
                }
                recoverUpdatedPage( page->header, copyPage->header );
                return( true );
            }
            return( false );
        }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>&&S<VT>),bool> = true >
        bool insert( const B<KT>* key, PageSize keySize, const V& value ) {
            Trail trail( *this );
            if (!lookUp<KT>( key, keySize, trail )) {
                if (stats) stats->insertions += 1;
                auto page = leaf( trail );
                if (!page->entryFit( keySize )) {
                    page = allocate<KT,VT>( trail, key, keySize, page->entryFilling( keySize ) );
                }
                auto copyPage = updatePage<VT>( trail );
                if (trail.atSplit()) {
                    page->insert( 0, key, keySize, value, copyPage );
                } else {
                    page->insert( (trail.index() + 1), key, keySize, value, copyPage );
                }
                recoverUpdatedPage( page->header, copyPage->header );
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
            if (!lookUp<KT>( key, trail )) {
                if (stats) stats->insertions += 1;
                auto page = leaf( trail );
                if (!page->entryFit( valueSize )) {
                    page = allocate<KT,VT>( trail, key, page->entryFilling( valueSize ) );
                }
                auto copyPage = updatePage<VT>( trail );
                if (trail.atSplit()) {
                    page->insert( 0, key, value, valueSize, copyPage );
                } else {
                    page->insert( (trail.index() + 1), key, value, valueSize, copyPage );
                }
                recoverUpdatedPage( page->header, copyPage->header );
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
            if (!lookUp<KT>( key, keySize, trail )) {
                if (stats) stats->insertions += 1;
                auto page = leaf( trail );
                if (!page->entryFit( keySize, valueSize )) {
                    page = allocate<KT,VT>( trail, key, keySize, page->entryFilling( keySize, valueSize ) );
                }
                auto copyPage = updatePage<VT>( trail );
                if (trail.atSplit()) {
                    page->insert( 0, key, keySize, value, valueSize, copyPage );
                } else {
                    page->insert( (trail.index() + 1), key, keySize, value, valueSize, copyPage );
                }
                recoverUpdatedPage( page->header, copyPage->header );
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
        // O(logN(n)*log(N)) complexity.
        template< class KT = K, class VT = V, std::enable_if_t<(S<KT>&&S<VT>),bool> = true >
        bool replace( const K& key, const V& value ) {
            Trail trail( *this );
            if (lookUp<KT>( key, trail )) {
                if (stats) stats->replacements += 1;
                auto page = leaf( trail );
                auto copyPage = updatePage<VT>( trail );
                if (trail.atSplit()) {
                    page->split( value, copyPage );
                } else {
                    page->replace( trail.index(), value, copyPage );
                }
                recoverUpdatedPage( page->header, copyPage->header );
                return( true );
            }
            return( false );
        }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>&&S<VT>),bool> = true >
        bool replace( const B<KT>* key, PageSize keySize, const V& value ) {
            Trail trail( *this );
            if (lookUp<KT>( key, keySize, trail )) {
                if (stats) stats->replacements += 1;
                auto page = leaf( trail );
                auto copyPage = updatePage<VT>( trail );
                if (trail.atSplit()) {
                    page->split( value, copyPage );
                } else {
                    page->replace( trail.index(), value, copyPage );
                }
                recoverUpdatedPage( page->header, copyPage->header );
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
            if (lookUp<KT>( key, trail )) {
                if (stats) stats->replacements += 1;
                auto page = leaf( trail );
                PageSize oldSize = ((trail.atSplit()) ? page->splitValueSize() : page->valueSize( trail.index() ));
                if ((oldSize < (valueSize * sizeof(B<VT>))) && (page->header.capacity < (page->filling() + ((valueSize * sizeof(B<VT>)) - oldSize)))) {
                    page = allocate<KT,VT>( trail, key, ((valueSize * sizeof(B<VT>)) - oldSize) );
                }
                auto copyPage = updatePage<VT>( trail );
                if (trail.atSplit()) {
                    page->split( value, valueSize, copyPage );
                } else {
                    page->replace( trail.index(), value, valueSize, copyPage );
                }
                recoverUpdatedPage( page->header, copyPage->header );
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
            if (lookUp<KT>( key, keySize, trail )) {
                if (stats) stats->replacements += 1;
                auto page = leaf( trail );
                PageSize oldSize = ((trail.atSplit()) ? page->splitValueSize() : page->valueSize( trail.index() ));
                if ((oldSize < (valueSize * sizeof(B<VT>))) && (page->header.capacity < (page->filling() + ((valueSize * sizeof(B<VT>)) - oldSize)))) {
                    page = allocate<KT,VT>( trail, key, keySize, ((valueSize * sizeof(B<VT>)) - oldSize) );
                }
                auto copyPage = updatePage<VT>( trail );
                if (trail.atSplit()) {
                    page->split( value, valueSize, copyPage );
                } else {
                    page->replace( trail.index(), value, valueSize, copyPage );
                }
                recoverUpdatedPage( page->header, copyPage->header );
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
        // O(logN(n)*log(N)) complexity.
        template< class KT = K, class VT = V, std::enable_if_t<(S<KT>&&S<VT>),bool> = true >
        const VT& at( const KT& key) const {
            static const char* signature = "const V& Tree<K,V>::at( const K& key) const";
            if (stats) stats->retrievals += 1;
            Trail trail( *this );
            if (!lookUp<KT>( key, trail )) throw std::string( signature ) + " - Key not found";
            return value<VT>( trail );
        }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>&&S<VT>),bool> = true >
        const VT& at( const B<KT>* key, PageSize keySize ) const {
            static const char* signature = "const V& Tree<K,V>::at( const K* key, PageSize keySize ) const";
            if (stats) stats->retrievals += 1;
            Trail trail( *this );
            if (!lookUp<KT>( key, keySize, trail )) throw std::string( signature ) + " - Key not found";
            return value<VT>( trail );
        }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>&&S<VT>),bool> = true >
        const VT& at( const std::pair< const B<KT>*, PageSize >& key ) const {
            return at( key.first, key.second );
        }
        template< class KT = K, class VT = V, std::enable_if_t<(S<KT>&&A<VT>),bool> = true >
        std::pair<const B<VT>*, PageSize> at( const KT& key) const {
            static const char* signature = "std::pair<const V*, PageSize> Tree<K,V>::at( const K& key) const";
            if (stats) stats->retrievals += 1;
            Trail trail( *this );
            if (!lookUp<KT>( key, trail )) throw std::string( signature ) + " - Key not found";
            return value<VT>( trail );
        }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>&&A<VT>),bool> = true >
        std::pair<const B<VT>*, PageSize> at( const B<KT>* key, PageSize keySize ) const {
            static const char* signature = "std::pair<const V*, PageSize> Tree<K,V>::at( const K* key, PageSize keySize ) const";
            if (stats) stats->retrievals += 1;
            Trail trail( *this );
            if (!lookUp<KT>( key, keySize, trail )) throw std::string( signature ) + " - Key not found";
            return value<VT>( trail );
        }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>&&A<VT>),bool> = true >
        std::pair<const B<VT>*, PageSize> at( const std::pair< const B<KT>*, PageSize >& key ) const {
            return at( key.first, key.second );
        }

        // Remove a key-value pair.
        // Returns true if key-value pair was removed, false if key-value pair was not found.
        // O(logN(n)*log(N)) complexity.
        template< class KT = K, std::enable_if_t<(S<KT>),bool> = true >
        bool erase( const KT& key ) {
            Trail trail( *this );
            if (!lookUp<KT>( key, trail )) return false;
            if (stats) stats->removals += 1;
            removeAt<KT,V>( trail );
            return true;
        }
        template< class KT = K, std::enable_if_t<(A<KT>),bool> = true >
        bool erase( const B<KT>* key, PageSize keySize ) {
            Trail trail( *this );
            if (!lookUp<KT>( key, keySize, trail )) return false;
            if (stats) stats->removals += 1;
            removeAt<KT,V>( trail );
            return true;
        }
        template< class KT = K, std::enable_if_t<(A<KT>),bool> = true >
        bool erase( const std::pair< const B<KT>*, PageSize >& key ) {
            return erase( key.first, key.second );
        }

        // Return iterator to begin of B-Tree
        template< class KT = K, class VT = V, std::enable_if_t<(S<KT>&&S<VT>),bool> = true >
        TreeIterator<KT,VT, std::pair<const KT&, const VT&> > begin() const {
            auto iterator = new TreeIterator<KT,VT, std::pair<const KT&, const VT&> >( *this );
            return iterator->begin();
        }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>&&S<VT>),bool> = true >
        TreeIterator<KT,VT, std::pair<std::pair<const B<KT>*,PageSize>, const VT&> > begin() const {
            auto iterator = new TreeIterator<KT,VT, std::pair<std::pair<const B<KT>*,PageSize>, const VT&> >( *this );
            return iterator->begin();
        }
        template< class KT = K, class VT = V, std::enable_if_t<(S<KT>&&A<VT>),bool> = true >
        TreeIterator<KT,VT, std::pair<const KT&, std::pair<const B<VT>*,PageSize>> > begin() const {
            auto iterator = new TreeIterator<KT,VT, std::pair<const KT&, std::pair<const B<VT>*,PageSize>> >( *this );
            return iterator->begin();
        }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>&&A<VT>),bool> = true >
        TreeIterator<KT,VT, std::pair<std::pair<const B<KT>*,PageSize>, std::pair<const B<VT>*,PageSize>> > begin() const {
            auto iterator = new TreeIterator<KT,VT, std::pair<std::pair<const B<KT>*,PageSize>, std::pair<const B<VT>*,PageSize>> >( *this );
            return iterator->begin();
        }
        // Return iterator to end of B-Tree
        template< class KT = K, class VT = V, std::enable_if_t<(S<KT>&&S<VT>),bool> = true >
        TreeIterator<KT,VT, std::pair<const KT&, const VT&> > end() const {
            auto iterator = new TreeIterator<KT,VT, std::pair<const KT&, const VT&> >( *this );
            return iterator->end();
        }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>&&S<VT>),bool> = true >
        TreeIterator<KT,VT, std::pair<std::pair<const B<KT>*,PageSize>, const VT&> > end() const {
            auto iterator = new TreeIterator<KT,VT, std::pair<std::pair<const B<KT>*,PageSize>, const VT&> >( *this );
            return iterator->end();
        }
        template< class KT = K, class VT = V, std::enable_if_t<(S<KT>&&A<VT>),bool> = true >
        TreeIterator<KT,VT, std::pair<const KT&, std::pair<const B<VT>*,PageSize>> > end() const {
            auto iterator = new TreeIterator<KT,VT, std::pair<const KT&, std::pair<const B<VT>*,PageSize>> >( *this );
            return iterator->end();
        }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>&&A<VT>),bool> = true >
        TreeIterator<KT,VT, std::pair<std::pair<const B<KT>*,PageSize>, std::pair<const B<VT>*,PageSize>> > end() const {
            auto iterator = new TreeIterator<KT,VT, std::pair<std::pair<const B<KT>*,PageSize>, std::pair<const B<VT>*,PageSize>> >( *this );
            return iterator->end();
        }
        // Return iterator to location of key in B-Tree.
        // If key cannot be found, returns end.
        template< class KT = K, class VT = V, std::enable_if_t<(S<KT>&&S<VT>),bool> = true >
        TreeIterator<KT,VT, std::pair<const KT&, const VT&> > find( const KT& key ) const {
            Trail trail( *this );
            bool found = lookUp<KT>( key, trail );
            auto iterator = new TreeIterator<KT,VT, std::pair<const KT&, const VT&> >( *this );
            if (found) return iterator->position( trail );
            return iterator->end();
        }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>&&S<VT>),bool> = true >
        TreeIterator<KT,VT, std::pair<std::pair<const B<KT>*,PageSize>, const VT&> > find( const B<KT>* key, PageSize keySize ) const {
            Trail trail( *this );
            bool found = lookUp<KT>( key, keySize, trail );
            auto iterator = new TreeIterator<KT,VT, std::pair<std::pair<const B<KT>*,PageSize>, const VT&> >( *this );
            if (found) return iterator->position( trail );
            return iterator->end();
        }
        template< class KT = K, class VT = V, std::enable_if_t<(S<KT>&&A<VT>),bool> = true >
        TreeIterator<KT,VT, std::pair<const KT&, std::pair<const B<VT>*,PageSize>> > find( const KT& key ) const {
            Trail trail( *this );
            bool found = lookUp<KT>( key, trail );
            auto iterator = new TreeIterator<KT,VT, std::pair<const KT&, std::pair<const B<VT>*,PageSize>> >( *this );
            if (found) return iterator->position( trail );
            return iterator->end();
        }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>&&A<VT>),bool> = true >
        TreeIterator<KT,VT, std::pair<std::pair<const B<KT>*,PageSize>, std::pair<const B<VT>*,PageSize>> > find( const B<KT>* key, PageSize keySize ) const {
            Trail trail( *this );
            bool found = lookUp<KT>( key, keySize, trail );
            auto iterator = new TreeIterator<KT,VT, std::pair<std::pair<const B<KT>*,PageSize>, std::pair<const B<VT>*,PageSize>> >( *this );
            if (found) return iterator->position( trail );
            return iterator->end();
        }

        inline void rootUpdate( PageHeader* page ) {
            if (stats) stats->rootUpdates += 1;
            root = page;
        }
        // Empty the B-Tree
        // O(logN(n)) complexity.
        void clear() {
            freeAll( *root, true );
            rootUpdate( &allocateLeaf()->header );
        }
        // Test if B-Tree is empty
        // O(1) complexity.
        bool empty() { return ((root->count == 0) && (root->split == 0)); }
        // Assign content of B-Tree
        // O(n*logN(n)) complexity.
        void assign( const Tree<K,V>& tree ) {
            clear();
            for ( auto src : tree ) insert( src.first, src.second );
        }
        // Assignment operator
        Tree<K,V>& operator=( const Tree<K,V>& tree ) { assign( tree ); return *this; }
        // Indexing operator
        template< class KT = K, class VT = V, std::enable_if_t<(S<KT>&&S<VT>),bool> = true >
        inline const VT& operator[]( const KT& key ) const { return at( key ); }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>&&S<VT>),bool> = true >
        inline const VT& operator[]( const std::pair<const B<KT>*,PageSize> key ) const { return at( key ); }
        template< class KT = K, class VT = V, std::enable_if_t<(S<KT>&&A<VT>),bool> = true >
        inline std::pair<const B<VT>*,PageSize> operator[]( const KT& key ) const { return at( key ); }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>&&A<VT>),bool> = true >
        inline std::pair<const B<VT>*,PageSize> operator[]( const std::pair<const B<KT>*,PageSize> key ) const { return at( key ); }
        // Return the number of key-value entries in the B-tree.
        // O(logN(n)) complexity.
        size_t size() const { return( pageCount( *root ) ); }
        // Test if the B-Tree contains a particular key.
        // O(logN(n)*log(N)) complexity.
        template< class KT = K, class VT = V, std::enable_if_t<(S<KT>),bool> = true >
        bool contains( const KT& key ) const {
            Trail trail( *this );
            return( lookUp<KT>( key, trail ) );
        }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>),bool> = true >
        bool contains( const B<KT>* key, PageSize keySize ) const {
            Trail trail( *this );
            return( lookUp<KT>( key, keySize, trail ) );
        }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>),bool> = true >
        bool contains( std::pair<const B<KT>*, PageSize>& key ) const {
            return contains( key.first, key.second );
        }
        template< class KT = K, class VT = V, std::enable_if_t<(S<KT>),bool> = true >
        inline PageSize count( const KT& key ) const { return( contains( key ) ? 1 : 0 ); }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>),bool> = true >
        inline PageSize count( const B<KT>* key, PageSize keySize ) const { return( contains( key, keySize ) ? 1 : 0 ); }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>),bool> = true >
        inline PageSize count( std::pair<const B<KT>*, PageSize>& key ) const { return( contains( key ) ? 1 : 0 ); }
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
            if (stats) stats->pageAllocations += 1;
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
                rootUpdate( &( const_cast<PageHeader&>(header) ) );
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
                    recoverPage( node->header, (mode != MemoryTransaction) );
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
        // The functions updatePage and recoverUpdatedPage are used to enclose code that updates
        // the content of a page as follows:
        //   Page<K,V,KA,VT>* page = ...
        //   Page<K,V,KA,VT>* copy = updatePage<V>( trail );
        //      ... code that updates page such as
        //      page->insert( index, key, value, copy );
        //      ...
        //   recoverUpdatedPage( page->header, copy->header );
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
        inline void recoverUpdatedPage( const PageHeader& src, const PageHeader& dst ) {
            if (src.page != dst.page) recoverPage( src, (mode != MemoryTransaction) );
        }
        inline void recoverPage( const PageHeader& page, bool free ) const {
            if (stats && free) stats->pageFrees += 1;
            pool.recover( page, free );
        }

        // Find a particular key.
        // Returns true if the key was found, false otherwise.
        // As a result, the trail will index a (found) key in a leaf page.
        // If key is associated with a split value, trail index will be 0 and compare negative.
        // For split values, a found key will reside in a node page eariler in trail.
        // If key is not found the indexed key is the largest key less than the requested key.
        // The optional depth indicates to which BTree depth to look-up. Defaults to 0, looking-up to leaf.
        // Find a scalar key.
        template< class KT, std::enable_if_t<(S<KT>),bool> = true >
        bool lookUp( const KT& key, Trail& trail, PageDepth to = 0 ) const {
            if (stats) stats->finds += 1;
            return recursiveLookUp<KT>( key, trail, to );
        }
        // Find an array key.
        template< class KT, std::enable_if_t<(A<KT>),bool> = true >
        bool lookUp( const B<KT>* key, PageSize keySize, Trail& trail, PageDepth to = 0 ) const {
            if (stats) stats->finds += 1;
            return recursiveLookUp<KT>( key, keySize, trail, to );
        }
        template< class KT, class VT, std::enable_if_t<(S<KT>),bool> = true >
        bool lookUp( const Page<B<KT>,B<VT>,false,A<VT>>* page, Trail& trail ) const {
            return lookUp<KT>( page->key( 0 ), trail, page->header.depth );
        }
        template< class KT, class VT, std::enable_if_t<(A<KT>),bool> = true >
        bool lookUp( const Page<B<KT>,B<VT>,true,A<VT>>* page, Trail& trail ) const {
            return lookUp<KT>( page->key( 0 ), page->keySize( 0 ), trail, page->header.depth );
        }
        template< class KT, std::enable_if_t<(S<KT>),bool> = true >
        inline bool recursiveLookUp( const KT& key, Trail& trail, PageDepth to = 0 ) const {
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
                    recursiveLookUp<KT>( key, trail.push( pool.reference( link ) ), to );
                    found = true;
                } else if (trail.atSplit() || ((nodePage->header.count == 0) && nodePage->splitDefined())) {
                    PageLink link = nodePage->split();
                    keyFound = recursiveLookUp<KT>( key, trail.push( pool.reference( link ) ), to );
                    found |= keyFound;
                } else if (0 <= trail.index()) {
                    PageLink link = nodePage->value( trail.index() );
                    keyFound = recursiveLookUp<KT>( key, trail.push( pool.reference( link ) ), to );
                    found |= keyFound;
                }
            }
            return found;
        }
        // Find an array key.
        template< class KT, std::enable_if_t<(A<KT>),bool> = true >
        inline bool recursiveLookUp( const B<KT>* key, PageSize keySize, Trail& trail, PageDepth to = 0 ) const {
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
                    recursiveLookUp<KT>( key, keySize, trail.push( pool.reference( link ) ), to );
                    found = true;
                } else if (trail.atSplit() || ((nodePage->header.count == 0) && nodePage->splitDefined())) {
                    PageLink link = nodePage->split();
                    keyFound = recursiveLookUp<KT>( key, keySize, trail.push( pool.reference( link ) ), to );
                    found |= keyFound;
                } else if (0 <= trail.index()) {
                    PageLink link = nodePage->value( trail.index() );
                    keyFound = recursiveLookUp<KT>( key, keySize, trail.push( pool.reference( link ) ), to );
                    found |= keyFound;
                }
            }
            return found;
        }

    private:
    
        // Insert a key-link in a Node Page.
        // Recursively reorganize tree as required, this can fail if maximum tree depth is exceeded.
        template< class KT, std::enable_if_t<(S<KT>),bool> = true >
        void nodeInsert( Trail& trail, const K& key, PageLink link ) {
            static const char* signature( "void Tree<K,V>::nodeInsert( Trail& trail, const K& key, PageLink link )" );
            if (trail.depth() == 0) {
                if (Trail::MaxHeight < (root->depth + 1)) throw std::string( signature ) + " - Max BTree heigth exceeded";
                PageHeader* oldRoot = root;
                auto node = allocateNode( root->depth + 1 );
                rootUpdate( &node->header );
                node->split( oldRoot->page );
                node->insert( 0, key, link );
                trail.clear();
            } else {
                auto parent = node( trail );
                if (!parent->entryFit()) {
                    parent = allocate<KT,PageLink>( trail, key, parent->entryFilling() );
                }
                auto copy = updatePage<PageLink>( trail );
                if ((parent->header.count == 0) or (trail.atSplit())) {
                    parent->insert( 0, key, link, copy );
                } else {
                    parent->insert( (trail.index() + 1), key, link, copy );
                }
                recoverUpdatedPage( parent->header, copy->header );
            }
        }
        template< class KT, std::enable_if_t<(A<KT>),bool> = true >
        void nodeInsert( Trail& trail, const B<KT>* key, PageSize keySize, PageLink link ) {
            static const char* signature( "void Tree<K,V>::nodeInsert( Trail& trail, const K* key, PageSize keySize, PageLink link )" );
            if (trail.depth() == 0) {
                if (Trail::MaxHeight < (root->depth + 1)) throw std::string( signature ) + " - Max BTree heigth exceeded";
                PageHeader* oldRoot = root;
                auto parent = allocateNode( root->depth + 1 );
                rootUpdate( &parent->header );
                parent->split( oldRoot->page );
                parent->insert( 0, key, keySize, link );
                trail.clear();
            } else {
                auto parent = node( trail );
                if (!parent->entryFit( keySize )) {
                    parent = allocate<KT,PageLink>( trail, key, keySize, parent->entryFilling( keySize ) );
                }
                auto copy = updatePage<PageLink>( trail );
                if ((parent->header.count == 0) or (trail.atSplit())) {
                    parent->insert( 0, key, keySize, link, copy );
                } else {
                    parent->insert( (trail.index() + 1), key, keySize, link, copy );
                }
                recoverUpdatedPage( parent->header, copy->header );
            }
        }
        template< class VT, std::enable_if_t<(S<VT>),bool> = true >
        inline void setIndexedSplit( Page<B<K>,VT,A<K>,false>& dst, const Page<B<K>,VT,A<K>,false>& src, PageIndex index ) const {
            dst.split( src.value( index ) );
        }
        template< class VT, std::enable_if_t<(A<VT>),bool> = true >
        inline void setIndexedSplit( Page<B<K>,B<VT>,A<K>,true>& dst, const Page<B<K>,B<VT>,A<K>,true>& src, PageIndex index ) const {
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
        // Derive index spliting a page into two pages of (nearly) equal filling.
        // This is not necessarily half-way in entries for pages with variable size keys and/or values.
        template< class KT, class VT >
        PageIndex optimalCut( const Page<B<KT>,B<VT>,A<KT>,A<VT>>& node ) {
            struct PageSplit {
                const Page<B<KT>,B<VT>,A<KT>,A<VT>>& node;
                const PageSize fill;
                PageSize left;
                PageSize right;
                PageSplit( const Page<B<KT>,B<VT>,A<KT>,A<VT>>& page ) : node( page ), fill( node.filling() ) {}
                PageSize cut( PageIndex index ) {
                    left = node.filling( index );
                    right = (fill - left + sizeof( PageHeader ));
                    return( (left <= right) ? (right - left) : (left - right));
                }
            } split( node );
            PageIndex probe = (node.header.count / 2);
            PageSize balance = split.cut( probe );
            while ((split.left < split.right) && ((probe + 1) < node.header.count)) {
                PageSize probeBalance = split.cut( probe + 1 );
                if (balance <= probeBalance) return( probe );
                balance = probeBalance;
                probe += 1;
            }
            while ((split.right < split.left) && (0 < (probe - 1))) {
                PageSize probeBalance = split.cut( probe - 1 );
                if (balance <= probeBalance) return( probe );
                balance = probeBalance;
                probe -= 1;
            }
            return( probe );
        }
        // Grow B-Tree by adding a page to the right of current page
        template< class VT >
        void grow( Trail& trail ) {
            if (stats) stats->grows += 1;
            auto node = page<VT>( trail );
            auto copyNode = updatePage<VT>( trail );
            auto right = allocatePage<VT>( node->header );
            node->shiftRight( *right, optimalCut<K,VT>( *node ), copyNode, right );
            PageIndex splitKeyIndex = (copyNode->header.count - 1);
            setIndexedSplit<VT>( *right, *copyNode, splitKeyIndex );
            insertSplitKey<K,VT>( trail.pop(), *copyNode, splitKeyIndex, right->header.page );
            copyNode->erase( splitKeyIndex );
            recoverUpdatedPage( node->header, copyNode->header );
        }
        template< class KT, class VT >
        void createRoom( Trail& trail, PageSize amount ) {
            auto header = trail.header();
            auto page = pool.page<B<KT>,B<VT>,A<KT>,A<VT>>( header );
            if (header->capacity < (page->filling() + amount)) {
/*
                // Look for sufficient room to left and/or right.
                // Must be able to shift at least one key-value to left or right
                // If there is room try to balance available room over all three pages.
                auto leftTrail = Trail( trail );
                auto leftPage = previousPage<VT>( leftTrail );
                if (leftPage) {
                    auto ancestor = node( trail, trail.match() );
                    PageSize leftRoom = (pool.pageCapacity() - leftPage->filling());
                }
                auto rightTrail = Trail( trail );
                auto rightPage = nextPage<VT>( rightTrail );
                if (rightPage) {
                    PageSize rightRoom = (pool.pageCapacity() - rightPage->filling());
                }
*/
                grow<VT>( trail );
            }
        }
        // Allocate room in Page for insertion by either:
        // a) shifting content to one or both neighboring pages
        // b) adding a page
        // Shifting content results is modifying keys in parent/ancestor pages.
        // This can result, recursively, in growing ancestor pages.
        // A page is only added if shifting content cannot create sufficient
        // room for the insertion. Pages are added to the right resulting in adding a
        // key to the parent/ancestor page. Content is distributed equally between
        // current .
        template< class KT, class VT, std::enable_if_t<(S<KT>),bool> = true >
        Page<B<KT>,B<VT>,false,A<VT>>* allocate( Trail& trail, const B<KT>& key, PageSize amount ) {
            PageDepth depth = trail.header()->depth;
            createRoom<KT,VT>( trail, amount );
            lookUp<KT>( key, trail.clear(), depth );
            return pool.page<B<KT>,B<VT>,false,A<VT>>( trail.header() );
        }
        template< class KT, class VT, std::enable_if_t<(A<KT>),bool> = true >
        Page<B<KT>,B<VT>,true,A<VT>>*  allocate( Trail& trail, const B<KT>* key, PageSize keySize, PageSize amount ) {
            PageDepth depth = trail.header()->depth;
            createRoom<KT,VT>( trail, amount );
            lookUp<KT>( key, keySize, trail.clear(), depth );
            return pool.page<B<KT>,B<VT>,true,A<VT>>( trail.header() );
        }

        // Remove current split and set split in Page to next key-value (if any)
        // Add new split key to appropriate node
        template< class KT, class VT >
        void nextSplit( Trail& trail ) {
            // Set split for fixed size keys
            auto page = this->page<VT>( trail );
            if (0 < page->header.count) {
                // Shift Page value 0 into split and update corresponding Node key to Page key 0
                // Shift into new page for convenience (minimize memory rearrangement)
                auto newPage = allocatePage<VT>( page->header );
                setIndexedSplit<VT>( *newPage, *page, 0 );
                if (1 < page->header.count) page->shiftRight( *newPage, 1 );
                replaceSplitKey<KT,VT>( trail, *page, 0, newPage->header.page );
                trail.pushSplit( &newPage->header );
                conditionalMerge( trail );
            } else {
                removeAt<KT,PageLink>( trail.pop() );
            }
            recoverPage( page->header, (mode != MemoryTransaction) );
        }

        // Remove entry at trail.
        template< class KT, class VT >
        void removeAt( Trail& trail ) {
            auto page = this->page<VT>( trail );
            if (trail.atIndex()) {
                // Removing a key-value pair in this page.
                PageIndex index = trail.index();
                auto copyPage = updatePage<VT>( trail );
                page->erase( index, copyPage );
                trail.deletedIndex();
                recoverUpdatedPage( page->header, copyPage->header );
                conditionalMerge( trail );
            } else {
                // Removing key to split value in this page.
                nextSplit<KT,VT>( trail );
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

        // Insert split key-value entry at index in dst
        // Key is indexed key in ancestor
        // Value is split value in src
        template< class VT, std::enable_if_t<(S<K>&&S<VT>),bool> = true >
        inline void insertSplit( Page<K,VT,false,false>& dst, PageIndex index, const Page<K,PageLink,false,false>& ancestor, PageIndex keyIndex, const Page<K,VT,false,false>& src, Page<K,VT,false,false>* copy ) {
            dst.insert( index, ancestor.key( keyIndex ), src.split(), copy );
        }
        template< class VT, std::enable_if_t<(S<K>&&A<VT>),bool> = true >
        inline void insertSplit( Page<K,B<VT>,false,true>& dst, PageIndex index, const Page<K,PageLink,false,false>& ancestor, PageIndex keyIndex, const Page<K,B<VT>,false,true>& src, Page<K,B<VT>,false,true>* copy ) {
            dst.insert( index, ancestor.key( keyIndex ), src.split(), src.splitSize(), copy );
        }
        template< class VT, std::enable_if_t<(A<K>&&S<VT>),bool> = true >
        inline void insertSplit( Page<B<K>,VT,true,false>& dst, PageIndex index, const Page<B<K>,PageLink,true,false>& ancestor, PageIndex keyIndex, const Page<B<K>,VT,true,false>& src, Page<B<K>,VT,true,false>* copy ) {
            dst.insert( index, ancestor.key( keyIndex ), ancestor.keySize( keyIndex ), src.split(), copy );
        }
        template< class VT, std::enable_if_t<(A<K>&&A<VT>),bool> = true >
        inline void insertSplit( Page<B<K>,B<VT>,true,true>& dst, PageIndex index, const Page<B<K>,PageLink,true,false>& ancestor, PageIndex keyIndex, const Page<B<K>,B<VT>,true,true>& src, Page<B<K>,B<VT>,true,true>* copy ) {
            dst.insert( index, ancestor.key( keyIndex ), ancestor.keySize( keyIndex ), src.split(), src.splitSize(), copy );
        }

        // Replace current node split key in trail
        // Key is indexed in src
        template< class KT, class VT, std::enable_if_t<(S<KT>),bool> = true >
        inline void replaceSplitKey( Trail& trail, const Page<B<KT>,B<VT>,false,A<VT>>& src, PageIndex index, PageLink link ) {
            if (stats) stats->splitUpdates += 1;
            auto parent = node( trail.pop() );
            auto parentCopy = updatePage<PageLink>( trail );
            if (trail.atSplit()) {
                parent->split( link, parentCopy );
                PageDepth offset = trail.match();
                auto ancestor = node( trail, offset );
                PageIndex keyIndex = trail.index( offset );
                ancestor->replace( keyIndex, src.key(index), ancestor->value( keyIndex ) );
            } else {
                parent->replace( trail.index(), src.key(index), link, parentCopy );
            }
            recoverUpdatedPage( parent->header, parentCopy->header );
        }
        template< class KT, class VT, std::enable_if_t<(A<KT>),bool> = true >
        inline void replaceSplitKey( Trail& trail, const Page<B<KT>,B<VT>,true,A<VT>>& src, PageIndex index, PageLink link ) {
            if (stats) stats->splitUpdates += 1;
            auto parent = node( trail.pop() );
            PageDepth offset = trail.match();
            auto ancestor = node( trail, offset );
            // ToDo: Only rearrange if key actually increases in size
            if (ancestor->entryFit( src.keySize( index ) )) {
                auto parentCopy = updatePage<PageLink>( trail );
                if (trail.atSplit()) {
                    parent->split( link, parentCopy );
                    auto ancestor = node( trail, offset );
                    PageIndex keyIndex = trail.index( offset );
                    ancestor->replace( keyIndex, src.key( index ), src.keySize( index ), ancestor->value( keyIndex ) );
                } else {
                    parent->replace( trail.index(), src.key( index ), src.keySize( index ), link, parentCopy );
                }
                recoverUpdatedPage( parent->header, parentCopy->header );
            } else {
                // Need to allocate additional room in ancestor node to accomodate increased key size
                while (!ancestor->entryFit( src.keySize( index ) )) {
                    grow<PageLink>( trail.popToMatch() );
                    lookUp<KT>( src.key( index ), src.keySize( index ), trail.clear(), src.header.depth );
                    ancestor = node( trail, trail.pop().match() );
                }
                trail.pushIndex( &src.header, index, true );
                replaceSplitKey<KT,VT>( trail, src, index, link );
            }
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
        // Append indexed content of source Page left to destination Page, adjusting split key
        // of source page accordingly.
        // Content of source page from split up to indexed entry is shifted left to destination page.
        // Trail arguments point to split key of source/right page and destination/left page respectively.
        template< class KT, class VT>
        void shiftPageLeft( Trail& dstTrail, Trail& srcTrail, PageIndex index ) {
            if (stats) stats->pageShifts += 1;
            const auto src = page<VT>( srcTrail );
            auto dst = page<VT>( dstTrail );
            auto dstCopy = updatePage<VT>( dstTrail );
            PageDepth offset = srcTrail.match();
            PageIndex splitKeyIndex = srcTrail.index( offset );
            Page<B<KT>,PageLink,A<KT>,false>* ancestor = page<PageLink>( srcTrail.header( offset ) );
            // Source page must have a split value as it cannot be leftmost leaf (i.e., is to right of destination).
            insertSplit<VT>( *dst, dst->header.count, *ancestor, splitKeyIndex, *src, dstCopy );
            src->shiftLeft( *dstCopy, index, const_cast<Page<B<KT>,B<VT>,A<KT>,A<VT>>*>(src), dstCopy );
            if (index < src->size()) {
                auto newSrc = allocatePage<VT>( src->header );
                setIndexedSplit( *newSrc, *src, index );
                src->shiftRight( *newSrc, newSrc->size() );
                replaceSplitKey<KT,VT>( srcTrail, *src, index, newSrc->header.page );
            } else {
                removeAt<KT,PageLink>( srcTrail.pop() );
            }
            recoverUpdatedPage( dst->header, dstCopy->header );
            recovePage( src->header, (mode != MemoryTransaction) );
        }
        // Append indexed content of source Page right to destination Page, adjusting split key
        // of destination page accordingly.
        // Content of source page from indexed entry to last entry is shifted right to destination page.
        // Trail arguments point to source/left page and destination/right page respectively.
        template< class KT, class VT>
        void shiftPageRight( Trail& srcTrail, PageIndex index, Trail& dstTrail ) {
            if (stats) stats->pageShifts += 1;
            auto src = page<VT>( srcTrail );
            auto srcCopy = updatePage<VT>( srcTrail );
            const auto dst = page<VT>( dstTrail );
            auto newDst = allocatePage<VT>( dst->header );
            PageDepth offset = dstTrail.match();
            PageIndex splitKeyIndex = dstTrail.index( offset );
            Page<B<KT>,PageLink,A<KT>,false>* ancestor = page<PageLink>( dstTrail.header( offset ) );
            // Shift to new page to avoid possible (temporary) overflow when split value decreases in size
            // Fill new page from low to high to avoid rearrangement
            setIndexedSplit( *newDst, *src, index );
            if ((index + 1) < src->size()) src->shifRight( *newDst, (index + 1), srcCopy, newDst );
            insertSplit<VT>( *newDst, newDst->size(), *ancestor, splitKeyIndex, *dst, newDst );
            dst->shiftLeft( *newDst, dst->size() );
            replaceSplitKey<KT,VT>( dstTrail, *srcCopy, index, newDst->header.page );
            srcCopy->erase( index );
            recovePage( dst->header, (mode != MemoryTransaction) );
            recoverUpdatedPage( src->header, srcCopy->header );
        }
        // Append entire content of src Page to dst Page, adjusting split key and
        // removing reference to (appended) src page.
        // Trail arguments point to source page and destination page respectively.
        // Merging is always from right/src to left/dst (using Page::shiftLeft)
        template< class KT, class VT>
        void mergePage( Trail& dstTrail, Trail& srcTrail ) {
            if (stats) stats->pageMerges += 1;
            const auto src = page<VT>( srcTrail );
            auto dst = page<VT>( dstTrail );
            auto dstCopy = updatePage<VT>( dstTrail );
            srcTrail.pop();
            dstTrail.pop();
            // Look for ancestor of page being merged.
            // This is a common ancestor of pages being merged. 
            PageDepth offset = srcTrail.match();
            PageIndex splitKeyIndex = srcTrail.index( offset );
            Page<B<KT>,PageLink,A<KT>,false>* ancestor = page<PageLink>( srcTrail.header( offset ) );
            // Page being merged must have a split value as it cannot be leftmost leaf.
            insertSplit<VT>( *dst, dst->header.count, *ancestor, splitKeyIndex, *src, dstCopy );
            if (0 < src->header.count) src->shiftLeft( *dstCopy, src->header.count );
            recoverUpdatedPage( dst->header, dstCopy->header );
            recoverPage( src->header, (mode != MemoryTransaction) );
            // A non-empty ancestor node must exist as we just merged two pages
            // Remove link to recently merged page
            pruneBranch( srcTrail );
            auto parent = node( srcTrail );
            if (parent == ancestor) {
                // Links to src and dst reside in single (parent) node.
                // Link to src is an indexed entry as it is to the right of link to dst
                ancestor->erase( splitKeyIndex );
                srcTrail.deletedIndex();
            } else {
                // Links to src and dst reside in separate (parent) nodes.
                // Link to src is split value of its parent.
                // Link to dst is last value of its parent.
                // Promote key 0 in parent to split key in ancestor
                auto keyPark = allocateNode( parent->header.depth, false );
                auto parentCopy = updatePage<PageLink>( srcTrail );
                insertSplit<PageLink>( *keyPark, 0, *parent, 0, *parent, keyPark );
                parent->split( parent->value( 0 ), parentCopy );
                parentCopy->erase( 0 );
                // Re-access ancestor as it may have changed due to copy-on-update of parent
                srcTrail.popToMatch();
                ancestor = page<PageLink>( srcTrail.header() );
                PageLink ancestorLink = ancestor->value( splitKeyIndex );
                ancestor->erase( splitKeyIndex );
                srcTrail.deletedIndex();
                insertSplitKey<KT,PageLink>( srcTrail, *keyPark, 0, ancestorLink );
                recoverUpdatedPage( parent->header, parentCopy->header );
                if (stats) stats->pageFrees += 1;
                pool.free( keyPark->header );
            }
            pruneRoot();
            // Recursively merge ancestor nodes where possible
            // Merged page (dstCopy) has at least one key-value entry, look it up using key( 0 )
            // ToDo: Find using key to parent of deleted page (possibly not key( 0 ))
            lookUp<KT,VT>( dstCopy, srcTrail.clear() );
            if (1 < srcTrail.depth()) {
                // Attempt to merge parent that just lost an entry.
                parent = node( srcTrail.pop() );
                if (parent->filling() < (LowPageThreshold * parent->header.capacity) ) merge<KT,PageLink>( srcTrail );
            }
        }
        // Prune tree of degenerate nodes up to non-empty node
        // A degenerate node being a node containing only a split link
        void pruneBranch( Trail& trail ) {
            auto node = page<PageLink>( trail );
            while (node->size() == 0) {
                recoverPage( node->header, (mode != MemoryTransaction) );
                node = page<PageLink>( trail.pop() );
            }
        }
        // Set new root if current root only has a split value.
        void pruneRoot() {
            while ((root->count == 0) && (0 < root->depth)) {
                auto rootPage = page<PageLink>( root );
                rootUpdate( pool.reference( rootPage->split() ) );
                recoverPage( rootPage->header, (mode != MemoryTransaction) );
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
            if (stats) stats->mergeAttempts += 1;
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
                PageSize threshold = static_cast<PageSize>(HighPageThreshold * page->header.capacity);
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
                if (recover) recoverPage( page, (mode != MemoryTransaction) );
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

        // ToDo: Avoid public access to these constuctors only to be used by Forest
        template< class KT = K, class VT = V >
        Tree(
            std::enable_if_t<(S<KT>),PagePool>& pagePool,
            int(*compareKey)( const B<KT>&, const B<KT>& ),
            UpdateMode updateMode,
            PageHeader* pageRoot
        ) : TreeBase( pagePool, pageRoot, updateMode ) {
            if (root == nullptr) rootUpdate( &allocateLeaf()->header );
            compare.scalar = compareKey;
        }
        template< class KT = K, class VT = V >
        Tree(
            std::enable_if_t<(A<KT>),PagePool>& pagePool,
            int(*compareKey)( const B<KT>*, PageSize, const B<KT>*, PageSize ),
            UpdateMode updateMode,
            PageHeader* pageRoot
        ) : TreeBase( pagePool, pageRoot, updateMode ) {
            if (root == nullptr) rootUpdate( &allocateLeaf()->header );
            compare.array = compareKey;
        }

    protected:

        // Recover B-Tree to a root defined by a PageLink. 
        void recoverTree( PageLink link ) {
            if (link.null()) {
                // No previous B-Tree state, recover to empty BTree
                freeAll( *root, true );
                rootUpdate( &allocateLeaf()->header );
            } else {
                rootUpdate( pool.reference( link ) );
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