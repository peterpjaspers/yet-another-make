#ifndef BTREE_BTREE_H
#define BTREE_BTREE_H

#include <stdlib.h>
#include <cstdint>
#include <string.h>
#include <iostream>
#include <iomanip>

#include "Page.h"
#include "Compare.h"
#include "Trail.h"

// ToDo: B-Tree bi-directional Iterator. Make use of Trail.
// ToDo: Implement various update/access strategies:
//          1: in-place update, primarily for in-memory B-tree, no multithreaded access
//          2: copy page on update, enables concurrent update and read access
//          3: transaction controlled access, transaction completion consolidates updated pages
//          4: versioned B-tree with transaction controlled access, consolidating update generates new version
//             Read access never blocks but continues on existing version
//             Requires garbage collection of stale versions; i.e., versions no longer being accessed
//          5: Local update transaction access (questionable!).
//             To enable concurrent update access on disjunct parts of the B-tree

namespace BTree {

    // Default scalar key comparison function making use of less<T> on type of key.
    template< class K >
    inline int defaultCompareScalar( const K& a, const K& b ) {
        return( std::less<K>()( a, b ) ? -1 : ( std::less<K>()( b, a ) ? +1 : 0 ) );
    };
    // Default array key comparison function making use of less<T> on type of key array elements.
    template< class K >
    inline int defaultCompareArray( const K* a, PageIndex na, const K* b, PageIndex nb ) {
        PageIndex n = (( na < nb ) ? na : nb );
        for (PageIndex i = 0; i < n; i++) {
            if (std::less<K>()( a[ i ], b[ i ] )) {
                return -1;
            } else if (std::less<K>()( b[ i ], a[ i ] )) {
                return +1;
            }
        }
        return( ( na < nb ) ? -1 : ( (nb < na) ? +1 : 0 ) );
    };

    static const float LowPageThreshold = 0.3;
    static const float HighPageThreshold = 0.9;

    // Updates on a B-tree can be either in-place or with copy-on-update behavior.
    // Modifications to a B-tree may exhibit in-place or transcation behavior.
    // Transactions require copy-on-update for all page modifications.
    // For transaction behaviour:
    //   Modifications are consolidated with a call to commit(), transaction succeeds.
    //   Modifications are discarded with a call to recover(), transaction fails.
    // During a transaction modified pages are registered with a call to modify() and
    // pages to be recoverd (i.e., copied-on-updat) are registered with a call to recover().
    // Pages copied-on-update with a persistent pool may be reused reducing memory footprint
    // (particularly for large transactions).

    enum UpdateMode {
        Auto = 0,                  // Infered from pool.persistent() (false -> InPlace, true -> PersistentTransaction)
        InPlace = 1,               // No copy-on-update behaviour (default for non-persistent pool)
        MemoryTransaction = 2,     // copy-on-update behaviour without memory reuse
        PersistentTransaction = 3  // copy-on-update behaviour with memory reuse (default for persistent pool)
    };

    template< class K, class V, bool KA = false, bool VA = false > class Tree {
    private:
        // The PagePool from which to allocate Pages for this Tree.
        PagePool& pool;
        // The update mnode to be enforced
        UpdateMode mode;
        // The (user defined) comparison function on keys.
        // Defaults to use of operator<() on key values.
        union {
            int (*scalar)( const K&, const K& );
            int (*array)( const K*, PageSize, const K*, PageSize );
        } compare;
        // Default scalar value to be returned if scalar value key not found.
        V scalarDefault;
        // Default array value to be returned if array value key not found.
        std::pair<const V*,PageIndex> arrayDefault;
        // The root page (header) of the B-tree
        PageHeader* root; // Root page is always referenced
        template< class SK, class SV, bool SKA, bool SVA >
        friend std::ostream & operator<<( std::ostream & o, const Tree<SK,SV,SKA,SVA>& tree );
    public:
        Tree() = delete;
        template< bool AK = KA, bool AV = VA >
        Tree(
            typename std::enable_if<(!AK&&!AV),PagePool>::type& pagePool,
            UpdateMode updateMode = UpdateMode::Auto,
            const V& defaultValue = V(),
            int (*compareKey)( const K&, const K& ) = defaultCompareScalar<K>
        ) : pool( pagePool ), mode( deriveMode( updateMode, pagePool ) ) {
            root = pool.root();
            if (root == nullptr) root = &allocateLeaf()->header;
            compare.scalar = compareKey;
            scalarDefault = defaultValue;
        }
        template< bool AK = KA, bool AV = VA >
        Tree(
            typename std::enable_if<(!AK&&AV),PagePool>::type& pagePool,
            UpdateMode updateMode = UpdateMode::Auto,
            const V* defaultValue = nullptr,
            PageIndex defaultValueSize = 0,
            int (*compareKey)( const K&, const K& ) = defaultCompareScalar<K>
        ) : pool( pagePool ), mode( deriveMode( updateMode, pagePool ) ) {
            root = pool.root();
            if (root == nullptr) root = &allocateLeaf()->header;
            compare.scalar = compareKey;
            arrayDefault = std::make_pair( defaultValue, defaultValueSize );
        }
        template< bool AK = KA, bool AV = VA >
        Tree(
            typename std::enable_if<(AK&&!AV),PagePool>::type& pagePool,
            UpdateMode updateMode = UpdateMode::Auto,
            const V& defaultValue = V(),
            int (*compareKey)( const K*, PageIndex, const K*, PageIndex ) = defaultCompareArray<K>
        ) : pool( pagePool ), mode( deriveMode( updateMode, pagePool ) ) {
            root = pool.root();
            if (root == nullptr) root = &allocateLeaf()->header;
            compare.array = compareKey;
            scalarDefault = defaultValue;
        }
        template< bool AK = KA, bool AV = VA >
        Tree(
            typename std::enable_if<(AK&&AV),PagePool>::type& pagePool,
            UpdateMode updateMode = UpdateMode::Auto,
            const V* defaultValue = nullptr,
            PageIndex defaultValueSize = 0,
            int (*compareKey)( const K*, PageIndex, const K*, PageIndex ) = defaultCompareArray<K>
        ) : pool( pagePool ), mode( deriveMode( updateMode, pagePool ) ) {
            root = pool.root();
            if (root == nullptr) root = &allocateLeaf()->header;
            compare.array = compareKey;
            arrayDefault = std::make_pair( defaultValue, defaultValueSize );
        }

        // Set value for a particular key
        // Set scalar value for scalar key
        template< bool AK = KA, bool AV = VA > typename std::enable_if<(!AK&&!AV),void>::type
        insert( const K& key, const V& value ) {
            Trail trail( pool, root );
            bool found = find( key, trail );
            Page<K,V,false,false>* page = leaf( trail );
            if (!page->entryFit()) {
                grow<V,false>( trail );
                found = find( key, trail );
                page = leaf( trail );
            }
            Page<K,V,false,false>* copyPage = updatePage<V,false>( trail );
            if (found) {
                if (trail.split()) {
                    page->split( value, copyPage );
                } else {
                    page->replace( trail.index(), value, copyPage );
                }
            } else {
                if ((page->header.count == 0) or (trail.compare() < 0)) {
                    page->insert( 0, key, value, copyPage );
                } else {
                    page->insert( (trail.index() + 1), key, value, copyPage );
                }
            }
            recoverPage( page->header, copyPage->header );
        }
        // Set scalar value for array key
        template< bool AK = KA, bool AV = VA > typename std::enable_if<(AK&&!AV),void>::type
        insert( const K* key, PageSize keySize, const V& value ) {
            Trail trail( pool, root );
            bool found = find( key, keySize, trail );
            Page<K,V,true,false>* page = leaf( trail );
            if (!page->entryFit( keySize )) {
                grow<V,false>( trail );
                found = find( key, keySize, trail );
                page = leaf( trail );
            }
            Page<K,V,true,false>* copyPage = updatePage<V,false>( trail );
            if (found) {
                if (trail.split()) {
                    page->split( value, copyPage );
                } else {
                    page->replace( trail.index(), value, copyPage );
                }
            } else {
                if ((page->header.count == 0) or (trail.compare() < 0)) {
                    page->insert( 0, key, keySize, value, copyPage );
                } else {
                    page->insert( (trail.index() + 1), key, keySize, value, copyPage );
                }
            }
            recoverPage( page->header, copyPage->header );
        }
        // Set array value for scalar key
        template< bool AK = KA, bool AV = VA > typename std::enable_if<(!AK&&AV),void>::type
        insert( const K& key, const V* value, PageSize valueSize ) {
            Trail trail( pool, root );
            bool found = find( key, trail );
            Page<K,V,false,true>* page = leaf( trail );
            if (!page->entryFit( valueSize )) {
                grow<V,true>( trail );
                found = find( key, trail );
                page = leaf( trail );
            }
            Page<K,V,false,true>* copyPage = updatePage<V,true>( trail );
            if (found) {
                if (trail.split()) {
                    page->split( value, valueSize, copyPage );
                } else {
                    page->replace( trail.index(), value, valueSize, copyPage );
                }
            } else {
                if ((page->header.count == 0) or (trail.compare() < 0)) {
                    page->insert( 0, key, value, valueSize, copyPage );
                } else {
                    page->insert( (trail.index() + 1), key, value, valueSize, copyPage );
                }
            }
            recoverPage( page->header, copyPage->header );
        }
        // Set array value for array key
        template< bool AK = KA, bool AV = VA > typename std::enable_if<(AK&&AV),void>::type
        insert( const K* key, PageSize keySize, const V* value, PageSize valueSize ) {
            Trail trail( pool, root );
            bool found = find( key, keySize, trail );
            Page<K,V,true,true>* page = leaf( trail );
            if (!page->entryFit( keySize, valueSize )) {
                grow<V,true>( trail );
                found = find( key, keySize, trail );
                page = leaf( trail );
            }
            Page<K,V,true,true>* copyPage = updatePage<V,true>( trail );
            if (found) {
                if (trail.split()) {
                    page->split( value, valueSize, copyPage );
                } else {
                    page->replace( trail.index(), value, valueSize, copyPage );
                }
            } else {
                if ((page->header.count == 0) or (trail.compare() < 0)) {
                    page->insert( 0, key, keySize, value, valueSize, copyPage );
                } else {
                    page->insert( (trail.index() + 1), key, keySize, value, valueSize, copyPage );
                }
            }
            recoverPage( page->header, copyPage->header );
        }

        // Return value a particular key.
        // Returns default value is key was not found.
        // Return scalar value for scalar key
        template< bool AK = KA, bool AV = VA > const typename std::enable_if<(!AK&&!AV),V>::type&
        retrieve( const K& key) const {
            Trail trail( pool, root );
            if (find( key, trail )) return value<false>( trail );
            return( scalarDefault );
        }
        // Return scalar value for array key
        template< bool AK = KA, bool AV = VA > const typename std::enable_if<(AK&&!AV),V>::type&
        retrieve( const K* key, const PageIndex keySize ) const {
            Trail trail( pool, root );
            if (find( key, keySize, trail )) return value<false>( trail );
            return( scalarDefault );
        }
        // Return array value for scalar key
        template< bool AK = KA, bool AV = VA > typename std::enable_if<(!AK&&AV),std::pair<const V*, PageIndex>>::type
        retrieve( const K& key ) const {
            Trail trail( pool, root );
            if (find( key, trail )) return value<true>( trail );
            return( arrayDefault );
        }
        // Return array value for array key
        template< bool AK = KA, bool AV = VA > typename std::enable_if<(AK&&AV),std::pair<const V*, PageIndex>>::type
        retrieve( const K* key, const PageIndex keySize ) const {
            Trail trail( pool, root );
            if (find( key, keySize, trail )) return value<true>( trail );
            return( arrayDefault );
        }

        // Remove a key-value pair.
        // Returns true if key-value pair was removed, false if key-value pair was not found.
        // Remove value for scalar key
        template< bool AK = KA >
        typename std::enable_if<(!AK),bool>::type remove( const K& key ) {
            Trail trail( pool, root );
            if (!find( key, trail )) return false;
            Page<K,V,AK,VA>* page = this->page<V,VA>( trail );
            if (trail.compare() == 0) {
                // Removing a key-value pair in this page.
                Page<K,V,false,VA>* copyPage = updatePage<V,VA>( trail );
                page->remove( trail.index(), copyPage );
                recoverPage( page->header, copyPage->header );
            } else {
                // Removing key to split value in this page.
                nextSplit<false>( trail, key );
                page = this->page<V,VA>( trail );
            }
            if (page->filling() < (LowPageThreshold * page->header.capacity) ) merge<V,AK,VA>( trail );
            return true;
        }
        // Remove value for array key
        template< bool AK = KA >
        typename std::enable_if<(AK),bool>::type remove( const K* key, PageIndex keySize ) {
            Trail trail( pool, root );
            if (!find( key, keySize, trail )) return false;
            Page<K,V,AK,VA>* page = this->page<V,VA>( trail );
            if (trail.compare() == 0) {
                // Removing a key-value pair in this page.
                Page<K,V,true,VA>* copyPage = updatePage<V,VA>( trail );
                page->remove( trail.index(), copyPage );
                recoverPage( page->header, copyPage->header );
            } else {
                // Removing key to split value in this page.
                nextSplit<true>( trail, key, keySize );
                page = this->page<V,VA>( trail );
            }
            if (page->filling() < (LowPageThreshold * page->header.capacity) ) merge<V,AK,VA>( trail );
            return true;
        }

        // Consolidate all B-tree modifications to a defined state.
        void commit() { pool.commit( root->page ); }
        // Restore B-tree to previously consolidated state.
        void recover() {
            PageLink link( pool.recover() );
            if (link.null()) {
                // No previous B-tree state, recover to empty B-tree
                freeAll( root );
                root = &allocateLeaf()->header;
            } else {
                root = pool.reference( link );
            }
        }

    private:

        // Allocate a new Page
        template<class VT = V, bool AV = VA>
        inline Page<K,VT,KA,AV>* allocatePage( PageDepth depth ) {
            Page<K,VT,KA,AV>* page = pool.page<K,VT,KA,AV>( depth );
            pool.modify( page->header );
            return( page );
        }
        inline Page<K,V,KA,VA>* allocateLeaf() {
            return allocatePage<V,VA>( 0 );
        }
        inline Page<K,PageLink,KA,false>* allocateNode( PageDepth depth ) {
            return( allocatePage<PageLink,false>( depth ) );
        }

        // Convert a PageHeader pointer to a Page instance.
        template<class VT = V, bool AV = VA>
        inline Page<K,VT,KA,AV>* page( const PageHeader* header ) const {
            return( pool.page<K,VT,KA,AV>( header ) );
        }
        // Convert a Trail to a Page instance.
        template<class VT = V, bool AV = VA>
        inline Page<K,VT,KA,AV>* page( const Trail& trail, PageDepth offset = 0 ) const {
            return( pool.page<K,VT,KA,AV>( trail.header( offset ) ) );
        }
        // Access Leaf Page addressed by Trail.
        // Returns null pointer if Trail does not address a Leaf Page.
        inline Page<K,V,KA,VA>* leaf( const Trail& trail ) const {
            const PageHeader* header = trail.header();
            if (header->depth != 0) return( nullptr );
            return( pool.page<K,V,KA,VA>( header ) );
        }
        // Access Node Page addressed by Trail at (optional) offset.
        // Returns null pointer if Trail does not address a Node Page.
        inline Page<K,PageLink,KA,false>* node( const Trail& trail, PageDepth offset = 0 ) const {
            const PageHeader* header = trail.header( offset );
            if (header->depth == 0) return( nullptr );
            return( pool.page<K,PageLink,KA,false>( header ) );
        }

        // Derive B-tree update mode as function of requested mode and page pool persistence.
        UpdateMode deriveMode( UpdateMode mode, PagePool& pool ) {
            if (mode != Auto) return mode;
            if (pool.persistent()) return PersistentTransaction;
            return InPlace;
        }

        // Recursively copy-on-update nodes in trail up to root.
        // Stop when a modified node or root is encountered.
        void updateNodeTrail( Trail& trail, PageHeader& header ) {
            PageIndex index = trail.index();
            KeyCompare compare = trail.compare();
            trail.pop();
            if (trail.depth() == 0) {
                root = &header;
            } else {
                Page<K,PageLink,KA,false>* node = page<PageLink,false>( trail.header() );
                if (node->header.modified) {
                    if (trail.split()) {
                        node->split( header.page );
                    } else {
                        node->replace( trail.index(), header.page );
                    }
                } else {
                    Page<K,PageLink,KA,false>* copy = allocatePage<PageLink,false>( node->header.depth );
                    if (trail.split()) {
                        node->split( header.page, copy );
                    } else {
                        node->replace( trail.index(), header.page, copy );
                    }
                    recoverPage( node->header, copy->header );
                    updateNodeTrail( trail, copy->header );
                }
            }
            trail.push( &header, index, compare );
        }

        // An update to the B-tree at trail is to be performed.
        // This can be an insertion, replacement or removal of a key-value.
        // Enforce update semantics being one of:
        //   InPlace -> no copy-on-update
        //   MemoryTransaction -> copy-on-update without memory reuse
        //   PersistentTransaction -> copy-on-update with memory reuse
        // Memory reuse (through recover) to be called after actual update is perfomed.
        // Does nothing if page is already modified; i.e., accessing copy of an earlier update.
        // The functions updatePage and recoverPage are used to enclose code that updates
        // the content of a page as follows:
        //   Page<K,VT,KA,AV>* page = ...
        //   Page<K,VT,KA,AV>* copy = updatePage<VT,AV>( trail );
        //      ... code that updatedates page such as
        //      page->insert( index, key, value, copy );
        //      ...
        //   recoverPage( page->header, copy->header );
        // The insert function is supplied with an additional argument for copy-on-update behavior.
        template< class VT, bool AV >
        Page<K,VT,KA,AV>* updatePage( Trail& trail ) {
            Page<K,VT,KA,AV>* page = this->page<VT,AV>( trail.header() );
            if (mode == InPlace) pool.modify( page->header );
            if (page->header.modified) return page;
            Page<K,VT,KA,AV>* copy = allocatePage<VT,AV>( page->header.depth );
            updateNodeTrail( trail, copy->header );
            return copy;
        }
        // Condition indicating that copy-on-update pages are to be reused (recycled as free page).
        inline bool reusePages() { return ((mode == PersistentTransaction) ? true : false ); }
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
        // The optional depth indicates to which B-tree depth to find. Defaults to 0, finding to leaf.
        // Find a scalar key.
        template< bool AK = KA > typename std::enable_if<(!AK),bool>::type
        find( const K& key, Trail& trail, PageDepth to = 0 ) const {
            bool found = false;
            bool keyFound;
            Page<K,V,false,VA>* leafPage = leaf( trail );
            if (leafPage) {
                keyFound = LeafCompareScalar<K,V,VA>( leafPage, key, compare.scalar ).index( trail );
                found |= keyFound;
            } else {
                Page<K,PageLink,false,false>* nodePage = node( trail );
                keyFound = NodeCompareScalar<K>( nodePage, key, compare.scalar ).index( trail );
                if (nodePage->header.depth == to) return found;
                if (trail.compare() == 0) {
                    PageLink link = nodePage->value( trail.index() );
                    find( key, trail.push( pool.reference( link ) ), to );
                    found = true;
                } else if (trail.split() || ((nodePage->header.count == 0) && nodePage->splitDefined())) {
                    PageLink link = nodePage->split();
                    keyFound = find( key, trail.push( pool.reference( link ) ), to );
                    found |= keyFound;
                } else if (0 <= trail.index()) {
                    PageLink link = nodePage->value( trail.index() );
                    keyFound = find( key, trail.push( pool.reference( link ) ), to );
                    found |= keyFound;
                }
            }
            return found;
        }
        // Find an array key.
        template< bool AK = KA > typename std::enable_if<(AK),bool>::type
        find( const K* key, const PageIndex keySize, Trail& trail, PageDepth to = 0 ) const {
            bool found = false;
            bool keyFound;
            Page<K,V,true,VA>* leafPage = leaf( trail );
            if (leafPage) {
                keyFound = LeafCompareArray<K,V,VA>( leafPage, key, keySize, compare.array ).index( trail );
                found |= keyFound;
            } else {
                Page<K,PageLink,true,false>* nodePage = node( trail );
                keyFound = NodeCompareArray<K>( nodePage, key, keySize, compare.array ).index( trail );
                if (nodePage->header.depth == to) return found;
                if (trail.compare() == 0) {
                    PageLink link = nodePage->value( trail.index() );
                    find( key, keySize, trail.push( pool.reference( link ) ), to );
                    found = true;
                } else if (trail.split() || ((nodePage->header.count == 0) && nodePage->splitDefined())) {
                    PageLink link = nodePage->split();
                    keyFound = find( key, keySize, trail.push( pool.reference( link ) ), to );
                    found |= keyFound;
                } else if (0 <= trail.index()) {
                    PageLink link = nodePage->value( trail.index() );
                    keyFound = find( key, keySize, trail.push( pool.reference( link ) ), to );
                    found |= keyFound;
                }
            }
            return found;
        }
        // Insert a key-link in a Node Page.
        // Insert scalar key-link
        template< bool AK = KA > typename std::enable_if<(!AK),void>::type
        nodeInsert( Trail& trail, const K& key, PageLink link ) {
            static const std::string signature( "void BTree<K,V,KA,VA>::nodeInsert( Trail& trail, const K& key, PageLink link )" );
            if (trail.depth() == 0) {
                if (MaxHeight < (root->depth + 1)) throw signature + " - Max B-tree heigth exceeded";
                PageLink linkRoot = root->page;
                Page<K,PageLink,false,false>* node = allocateNode( root->depth + 1 );
                root = &node->header;
                node->insert( 0, key, link );
                node->split( linkRoot );
                trail.clear( root );
            } else {
                Page<K,PageLink,false,false>* node = this->node( trail );
                if (!node->entryFit()) {
                    grow<PageLink,false>( trail );
                    find( key, trail, node->header.depth );
                    node = this->node( trail );
                }
                Page<K,PageLink,false,false>* copy = updatePage<PageLink,false>( trail );
                if ((node->header.count == 0) or (trail.compare() < 0)) {
                    node->insert( 0, key, link, copy );
                } else {
                    node->insert( (trail.index() + 1), key, link, copy );
                }
                recoverPage( node->header, copy->header );
            }
        }
        // Insert array key-link
        template< bool AK = KA > typename std::enable_if<(AK),void>::type
        nodeInsert( Trail& trail, const K* key, PageIndex keySize, PageLink link ) {
            static const std::string signature( "void BTree<K,V,KA,VA>::nodeInsert( Trail& trail, const K* key, PageIndex keySize, PageLink link )" );
            if (trail.depth() == 0) {
                if (MaxHeight < (root->depth + 1)) throw signature + " - Max B-tree heigth exceeded";
                PageLink linkRoot = root->page;
                Page<K,PageLink,true,false>* node = allocateNode( root->depth + 1 );
                root = &node->header;
                node->insert( 0, key, keySize, link );
                node->split( linkRoot );
                trail.clear( root );
            } else {
                Page<K,PageLink,true,false>* node = this->node( trail );
                if (!node->entryFit( keySize )) {
                    grow<PageLink,false>( trail );
                    find( key, keySize, trail, node->header.depth );
                    node = this->node( trail );
                }
                Page<K,PageLink,true,false>* copy = updatePage<PageLink,false>( trail );
                if ((node->header.count == 0) or (trail.compare() < 0)) {
                    node->insert( 0, key, keySize, link, copy );
                } else {
                    node->insert( (trail.index() + 1), key, keySize, link, copy );
                }
                recoverPage( node->header, copy->header );
            }
        }
        template< class VT = V, bool AV = VA >
        inline typename std::enable_if<(!AV),void>::type
        setSplit( Page<K,VT,KA,false>& dst, const Page<K,VT,KA,false>& src, PageIndex index ) const {
            dst.split( src.value( index ) );
        }
        template< class VT = V, bool AV = VA >
        inline typename std::enable_if<(AV),void>::type
        setSplit( Page<K,VT,KA,true>& dst, const Page<K,VT,KA,true>& src, PageIndex index ) const {
            dst.split( src.value( index ), src.valueSize( index ) );
        }
        template< class VT = V, bool AK = KA, bool AV = VA >
        inline typename std::enable_if<(!AK),void>::type
        insertSplitKey( Trail& trail, const Page<K,VT,false,AV>& src, PageIndex index, PageLink link ) {
            nodeInsert( trail, src.key( index ), link );
        }
        template< class VT = V, bool AK = KA, bool AV = VA >
        inline typename std::enable_if<(AK),void>::type
        insertSplitKey( Trail& trail, const Page<K,VT,true,AV>& src, PageIndex index, PageLink link ) {
            nodeInsert( trail, src.key( index ), src.keySize( index ), link );
        }
        // Create room in Page by adding a Page to the right
        // This will result in adding a key to the parent page.
        // Content is is shifted to the newly created Page, making room in both Pages.
        // This can fail if maximum tree depth is exceeded.
        template< class VT = V, bool AV = VA > void
        grow( Trail& trail ) {
            Page<K,VT,KA,AV>* node = page<VT,AV>( trail );
            Page<K,VT,KA,AV>* copy = updatePage<VT,AV>( trail );
            PageIndex index = (node->header.count / 2);
            Page<K,VT,KA,AV>* right = allocatePage<VT,AV>( node->header.depth );
            node->shiftRight( *right, index, copy );
            PageIndex splitIndex = (copy->header.count - 1);
            setSplit<VT,AV>( *right, *copy, splitIndex );
            insertSplitKey<VT,KA,AV>( trail.pop(), *copy, splitIndex, right->header.page );
            copy->remove( splitIndex );
            recoverPage( node->header, copy->header );
        }

        // Allocate room in ancestor node for (possibly) larger array key
        void allocateSplit( Trail& trail, const K* key, const PageSize keySize ) {
            Page<K,V,true,VA>* page = this->page<V,VA>( trail );
            PageDepth offset = trail.match();
            Page<K,PageLink,true,false>* ancestor = this->node( trail, offset );
            while (!ancestor->entryFit( page->keySize(0) )) {
                trail.popToMatch();
                grow<PageLink,false>( trail );
                find( key, keySize, trail, page->header.depth );
                offset = trail.match();
                ancestor = this->node( trail, offset );
            }
        }        
        
        // Set split value in Page with next largest key (if any)
        // Add new split key to appropriate node
        // Set next scalar key split
        template<bool AK = KA>
        typename std::enable_if<(!AK),void>::type nextSplit( Trail& trail, const K& key ) {
            Page<K,V,false,VA>* page = this->page<V,VA>( trail );
            if (0 < page->header.count) {
                // Shift Page key 0 into split and update corresponding Node key to match
                // Shift into new page for convenience (minimize memory rearrangement)
                Page<K,V,false,VA>* newPage = allocatePage<V,VA>( page->header.depth );
                setSplit<V,VA>( *newPage, *page, 0 );
                if (1 < page->header.count) page->shiftRight( *newPage, 1 );
                trail.pop();
                Page<K,PageLink,false,false>* node = this->node( trail );
                Page<K,PageLink,false,false>* copy = updatePage<PageLink,false>( trail );
                if (trail.split()) {
                    node->split( newPage->header.page, copy );
                    PageDepth offset = trail.match();
                    // Ancestor page is already copied due to earlier updatePage...
                    Page<K,PageLink,false,false>* ancestor = this->node( trail, offset );
                    PageIndex index = trail.index( offset );
                    ancestor->exchange( index, page->key(0), ancestor->value(index) );
                } else {
                    node->exchange( trail.index(), page->key(0), newPage->header.page, copy );
                }
                recoverPage( node->header, copy->header );
                trail.push( &newPage->header, 0, -1 );
                pool.recover( page->header, false );
                pool.free( page->header );
            } else {
                Page<K,V,false,VA>* copyPage = updatePage<V,VA>( trail );
                page->removeSplit( copyPage );
                recoverPage( page->header, copyPage->header );
            }
        }
        // Set next array key split
        template<bool AK = KA>
        typename std::enable_if<(AK),void>::type nextSplit( Trail& trail, const K* key, const PageSize keySize ) {
            Page<K,V,true,VA>* page = this->page<V,VA>( trail );
            if (0 < page->header.count) {
                // Shift Page key 0 into split and update corresponding Node key to match
                // Shift into new page for convenience (minimize memory rearrangement)
                Page<K,V,true,VA>* newPage = allocatePage<V,VA>( page->header.depth );
                setSplit<V,VA>( *newPage, *page, 0 );
                if (1 < page->header.count) page->shiftRight( *newPage, 1 );
                allocateSplit( trail, key, keySize );
                trail.pop();
                Page<K,PageLink,true,false>* node = this->node( trail );
                Page<K,PageLink,true,false>* copy = updatePage<PageLink,false>( trail );
                if (trail.split()) {
                    node->split( newPage->header.page, copy );
                    PageDepth offset = trail.match();
                    // Ancestor page is already copied due to earlier updatePage...
                    Page<K,PageLink,true,false>* ancestor = this->node( trail, offset );
                    PageIndex index = trail.index( offset );
                    ancestor->exchange( index, page->key(0), page->keySize(0), ancestor->value(index) );
                } else {
                    node->exchange( trail.index(), page->key(0), page->keySize(0), newPage->header.page, copy );
                }
                recoverPage( node->header, copy->header );
                trail.push( &newPage->header, 0, -1 );
                pool.recover( page->header, false );
                pool.free( page->header );
            } else {
                Page<K,V,true,VA>* copyPage = updatePage<V,VA>( trail );
                page->removeSplit( copyPage );
                recoverPage( page->header, copyPage->header );
            }
        }

        // Return scalar value at trail to leaf
        template< bool AV = VA > const typename std::enable_if<(!AV),V>::type&
        value( const Trail& trail ) const {
            Page<K,V,KA,false>* page = leaf( trail );
            if (trail.compare() == 0) return page->value( trail.index() );
            return page->split();
        }
        // Return array value at trail to leaf
        template< bool AV = VA > typename std::enable_if<(AV),std::pair<const V*, PageIndex>>::type
        value( const Trail& trail ) const {
            Page<K,V,KA,true>* page = leaf( trail );
            if (trail.compare() == 0) return{ page->value( trail.index() ), page->valueSize( trail.index() ) };
            return{ page->split(), page->splitSize() };
        }
        // Append split with value in src and key in ancestor at index to src.
        // Append scalar value with scalar key
        template< class VT, bool AK, bool AV > inline typename std::enable_if<(!AK&&!AV),void>::type
        appendSplit( Page<K,VT,AK,AV>& dst, const Page<K,PageLink,AK,false>& ancestor, PageIndex index, const Page<K,VT,AK,AV>& src, Page<K,VT,AK,AV>* copy = nullptr ) {
            dst.insert( dst.header.count, ancestor.key( index ), src.split(), copy );
        }
        // Append array value with scalar key
        template< class VT, bool AK, bool AV > inline typename std::enable_if<(!AK&&AV),void>::type
        appendSplit( Page<K,VT,AK,AV>& dst, const Page<K,PageLink,AK,false>& ancestor, PageIndex index, const Page<K,VT,AK,AV>& src, Page<K,VT,AK,AV>* copy = nullptr ) {
            dst.insert( dst.header.count, ancestor.key( index ), src.split(), src.splitSize(), copy );
        }
        // Append scalar value with array key
        template< class VT, bool AK, bool AV > inline typename std::enable_if<(AK&&!AV),void>::type
        appendSplit( Page<K,VT,AK,AV>& dst, const Page<K,PageLink,AK,false>& ancestor, PageIndex index, const Page<K,VT,AK,AV>& src, Page<K,VT,AK,AV>* copy = nullptr ) {
            dst.insert( dst.header.count, ancestor.key( index ), ancestor.keySize( index ), src.split(), copy );
        }
        // Append array value with array key
        template< class VT, bool AK, bool AV > inline typename std::enable_if<(AK&&AV),void>::type
        appendSplit( Page<K,VT,AK,AV>& dst, const Page<K,PageLink,AK,false>& ancestor, PageIndex index, const Page<K,VT,AK,AV>& src, Page<K,VT,AK,AV>* copy = nullptr ) {
            dst.insert( dst.header.count, ancestor.key( index ), ancestor.keySize( index ), src.split(), src.splitSize(), copy );
        }

        template< bool AK = KA > inline typename std::enable_if<(!AK),void>::type
        locateSplit( Trail& trail, const Page<K,PageLink,false,false>& node ) const {
            find( node.key( 0 ), trail );
        }
        template< bool AK = KA > inline typename std::enable_if<(AK),void>::type
        locateSplit( Trail& trail, const Page<K,PageLink,true,false>& node ) const {
            find( node.key( 0 ), node.keySize( 0 ), trail );
        }
        void removeSplitKey( const Trail& trail ) {
            PageDepth offset = trail.match();
            Page<K,PageLink,KA,false>* node = page<PageLink,false>( trail.header( offset ) );
            node->remove( trail.index( offset ) );
        }

        // Access previous page at trail (referencing node page)
        // Returns null if there is no previous page
        template< class VT = V, bool AV = VA>
        Page<K,VT,KA,AV>* previousPage( Trail& trail ) {
            static const std::string signature( "Page<K,VT,KA,AV>* BTree<K,V,KA,VA>::previousPage( Trail& trail )" );
            if ( trail.previousPage<K,KA>() ) {
                const PageHeader* header = trail.indexedHeader<K,KA>();
                if (0 < header->count) {
                    trail.push( header, (header->count - 1), 0 );
                } else {
                    if (header->split == 0) throw signature + " - Navigating to empty page";
                    trail.push( header, 0, -1 );
                }
                return page<VT,AV>( header );
            }
            return nullptr;
        }
        // Access next page at trail (referencing node page)
        // Returns null if there is no next page
        template< class VT = V, bool AV = VA>
        Page<K,VT,KA,AV>* nextPage( Trail& trail ) {
            static const std::string signature( "Page<K,VT,KA,AV>* BTree<K,V,KA,VA>::nextPage( Trail& trail )" );
            if ( trail.nextPage<K,KA>() ) {
                const PageHeader* header = trail.indexedHeader<K,KA>();
                if (0 < header->split) {
                    trail.push( header, 0 , -1 );
                } else {
                    if (header->count == 0) throw signature + " - Navigating to empty page";
                    trail.push( header, 0 , 0 );
                }
                return page<VT,AV>( header );
            }
            return nullptr;
        }
        // Append content of src Page to dst Page, adjusting split key and
        // removing node key to (appended) src page.
        // Trail points to parent of src.
        // Frees src Page
        template< class VT = V, bool AK = KA, bool AV = VA>
        void mergePage( Trail& srcTrail, Trail& dstTrail ) {
            Page<K,VT,AK,AV>* src = page<VT,AV>( srcTrail );
            Page<K,VT,AK,AV>* dst = page<VT,AV>( dstTrail );
            Page<K,VT,AK,AV>* srcCopy = updatePage<VT,AV>( srcTrail );
            Page<K,VT,AK,AV>* dstCopy = updatePage<VT,AV>( dstTrail );
            srcTrail.pop();
            dstTrail.pop();
            PageDepth offset = srcTrail.match();
            PageIndex index = srcTrail.index( offset );
            Page<K,PageLink,AK,false>* ancestor = page<PageLink,false>( srcTrail.header( offset ) );
            appendSplit<VT,AK,AV>( *dst, *ancestor, index, *src, dstCopy );
            src->shiftLeft( *dstCopy, src->header.count, srcCopy );
            if (srcTrail.split()) {
                // Replace split key with next largest key if any
                Page<K,PageLink,AK,false>* parent = node( srcTrail );
                if (0 < parent->header.count) {
                    Page<K,PageLink,AK,false>* park = allocateNode( ancestor->header.depth );
                    appendSplit<PageLink,AK,false>( *park, *ancestor, index, *parent );
                    parent->split( parent->value( 0 ) );
                    srcTrail.popToMatch();
                    insertSplitKey( srcTrail, *parent, 0, ancestor->value( index ) );
                    parent->remove( 0 );
                    srcTrail.clear( root );
                    locateSplit( srcTrail, *park );
                    removeSplitKey( srcTrail );
                    pool.free( park->header.page );
                }   
            } else {
                ancestor->remove( index );
            }
            recoverPage( dst->header, dstCopy->header );
            pool.recover( src->header, false );
            pool.free( src->header.page );
            if (srcTrail.depth() == 1) {
                if (ancestor->header.count == 0) {
                    // Merged tree to single leaf page
                    root = pool.reference( ancestor->split() );
                    pool.free( ancestor->header.page );
                }
            } else {
                srcTrail.popToMatch();
                ancestor = node( srcTrail );
                if (ancestor->filling() < (LowPageThreshold * ancestor->header.capacity) ) merge<PageLink,AK,false>( srcTrail );
            }
        }
        template< bool AK = KA > typename std::enable_if<(!AK),PageSize>::type
        splitKeySize( const Trail& trail ) { return sizeof(K); }
        template< bool AK = KA > typename std::enable_if<(AK),PageSize>::type
        splitKeySize( const Trail& trail ) {
            PageDepth offset = trail.match();
            Page<K,PageLink,AK,false>* ancestor = node( trail, offset );
            return( sizeof(K) * ancestor->keySize( trail.index( offset ) ) );
        }
        // Merge neighbouring Pages, if any
        // This may result recursively merging Pages higher up in B-tree, eventually reducing B-tree height.
        template< class VT = V, bool AK = KA, bool AV = VA>
        void merge( Trail& trail ) {
            const PageHeader* header = trail.header();
            if (header != root) {
                Trail pageTrail( trail );
                trail.pop();
                const Page<K,VT,KA,AV>* page = this->page<VT,AV>( header );
                PageIndex pageFill = page->filling();
                Trail leftTrail( trail );
                PageSize leftSplitKeySize = splitKeySize( leftTrail );
                const Page<K,VT,KA,AV>* leftPage = previousPage<VT,AV>( leftTrail );
                PageIndex leftFill = ( leftPage ? (leftPage->filling() + leftSplitKeySize) : (2 * page->header.capacity) );
                Trail rightTrail( trail );
                PageSize rightSplitKeySize = splitKeySize( rightTrail );
                const Page<K,VT,KA,AV>* rightPage = nextPage<VT,AV>( rightTrail );
                PageIndex rightFill = ( rightPage ? (rightPage->filling() + rightSplitKeySize) : (2 * page->header.capacity) );
                PageIndex threshold = (HighPageThreshold * page->header.capacity);
                if ((leftFill < rightFill) && ((leftFill + pageFill) < threshold)) {
                    mergePage<VT,AK,AV>( pageTrail, leftTrail ); // Shift content of this page to left
                } else if ((rightFill + pageFill) < threshold) {
                    mergePage<VT,AK,AV>( rightTrail, pageTrail ); // Shift content of right page to this page
                }
            }
        }

        // Free a page and all pages that is references; i.e., free an entire (sub-) tree.
        void freeAll( const PageHeader* page ) {
            if (0 < page->depth) {
                Page<K,PageLink,KA,false>* node = this->page<PageLink,false>( page );
                if (node->splitDefined()) freeAll( pool.reference( node->split() ) );
                for (PageIndex i = 0; i < node->size(); i++) freeAll( pool.reference( node->value( i ) ) );
            }
            pool.free( page );
        }

        void streamPage( std::ostream & o, PageLink link, PageDepth depth ) const {
            if (pool.valid( link )) {
                const PageHeader& header = pool.access( link );
                if (header.depth == 0) {
                    Page<K,V,KA,VA>* leaf = pool.page<K,V,KA,VA>( &header );
                    if (header.depth == depth) o << *leaf;
                } else {
                    Page<K,PageLink,KA,false>* node = pool.page<K,PageLink,KA,false>( &header );
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

    protected:

        void stream( std::ostream & o ) const {
            o << "Root\n";
            for (int d = root->depth; 0 <= d; d--) streamPage( o, root->page, d );
        }

    private:
    
        struct Iterator 
        {
            using iterator_category = std::forward_iterator_tag;
            using difference_type   = void;
            using value_type        = void;
            using pointer           = void;
            using reference         = void;

            Iterator( Trail location ) : trail( location ) {}

            Iterator& operator++() { trail.next<K,KA>(); return *this; }  
            Iterator operator++(int) { Iterator tmp = *this; ++(*this); return tmp; }
            Iterator& operator--() { trail.previous<K,KA>(); return *this; }  
            Iterator operator--(int) { Iterator tmp = *this; ++(*this); return tmp; }
            inline bool operator==( const Iterator& i ) { return( trail == i.trail ); }
            inline bool operator!= (const Iterator& i ) { return( trail != i.trail ); }

        private:
            Trail trail;
        };

        Iterator begin() {
            Trail trail;
            // Navigate trail to begin...
            return Iterator( trail );
        }
        Iterator end() {
            Trail trail;
            // Navigate trail to end...
            return Iterator( trail );            
        }

    }; // class Tree

    template< class K, class V, bool KA, bool VA >
    inline std::ostream & operator<<( std::ostream & o, const Tree<K,V,KA,VA>& tree ) { tree.stream( o ); return o; }




}; // namespace BTree

#endif // BTREE_BTREE_H