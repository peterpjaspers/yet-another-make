#ifndef BTREE_COMPARE_H
#define BTREE_COMPARE_H

#include <stdlib.h>
#include <cstdint>

#include "Page.h"
#include "Trail.h"

namespace BTree {

    // Compare functor operating on Page indeces.
    // Compares functor key with the key at argument index.
    // Derived classes operate on Leaf and Node pages with fixed and variable size keys.
    class Compare {
    public:
        virtual KeyCompare operator()( PageIndex index ) const { return 0; };
        virtual PageSize size() const { return 0; };
        // Determine page index of a key via compare functor.
        // Returns true if key was found, false otherwise.
        // If key was not found, returns index of smallest key larger than requested key
        // unless such a key does not exist in which case returns size of page.
        bool position( Trail& trail ) const {
            PageSize n = size();
            if (n == 0) {
                trail.position( Trail::AfterSplit );
                return false;
            }
            PageIndex high = n;
            PageIndex low = 0;
            PageIndex mid;
            KeyCompare cmp;
            do {
                mid = ((high + low) / 2);
                cmp = operator()( mid );
                if (cmp < 0) {
                    high = mid;
                } else if (0 < cmp) {
                    low = (mid + 1);
                } else {
                    trail.position( Trail::OnIndex, mid );
                    return true;
                }
            } while (low < high);
            PageIndex idx = mid;
            if (low == n) idx = (n - 1);
            else if (mid < low) idx = low;
            else if (0 < low) idx = (low - 1);
            cmp = operator()( idx );
            if (0 < cmp) {
                trail.position( Trail::AfterIndex, idx );
                return false;
            } else if (cmp < 0) {
                if (0 < idx) trail.position( Trail::AfterIndex, (idx - 1) );
                else trail.position( Trail::AfterSplit );
                return false;
            }
            trail.position( Trail::OnIndex, idx );
            return true;
        };
    };
    template< class K, class V, bool KA, bool VA > 
    class LeafCompare : public Compare {
    protected:
        const Page<K,V,KA,VA>* page;
    public:
        LeafCompare() = delete;
        LeafCompare( const Page<K,V,KA,VA>* page ) { this->page = page; };
        virtual KeyCompare operator()( PageIndex index ) const { return 0; };
        PageSize size() const { return page->header.count; };
    };
    template< class K, bool KA > 
    class NodeCompare : public Compare {
    protected:
        const Page<K,PageLink,KA,false>* page;
    public:
        NodeCompare() = delete;
        NodeCompare( const Page<K,PageLink,KA,false>* page ) { this->page = page; };
        virtual KeyCompare operator()( PageIndex index ) const { return 0; };
        PageSize size() const { return page->header.count; };
    };
    template< class K, class V, bool VA > 
    class LeafCompareScalar : public LeafCompare<K,V,false, VA> {
        const K& key;
        KeyCompare (*compare)( const K&, const K& );
    public:
        LeafCompareScalar() = delete;
        LeafCompareScalar( const Page<K,V,false,VA>* page, const K& k, KeyCompare (*compare)( const K&, const K& ) ) :
        LeafCompare<K,V,false,VA>( page ), key(k) { this->compare = compare; };
        KeyCompare operator()( PageIndex index ) const { return compare( key, this->page->key( index ) ); };
    };
    template< class K > 
    class NodeCompareScalar : public NodeCompare<K,false> {
        const K& key;
        KeyCompare (*compare)( const K&, const K& );
    public:
        NodeCompareScalar() = delete;
        NodeCompareScalar( const Page<K,PageLink,false,false>* page, const K& k, KeyCompare (*compare)( const K&, const K& ) ) :
        NodeCompare<K,false>( page ), key( k ) { this->compare = compare; };
        KeyCompare operator()( PageIndex index ) const { return compare( key, this->page->key( index ) ); };
    };
    template< class K, class V, bool VA > 
    class LeafCompareArray : public LeafCompare<K,V,true,VA> {
        const K* key;
        const PageSize keySize;
        KeyCompare (*compare)( const K*, PageSize, const K*, PageSize );
    public:
        LeafCompareArray() = delete;
        LeafCompareArray( const Page<K,V,true,VA>* page, const K* k, const PageSize kS, KeyCompare (*compare)( const K*, PageSize, const K*, PageSize ) ) :
        LeafCompare<K,V,true,VA>( page ), key( k ), keySize( kS ) { this->compare = compare; };
        KeyCompare operator()( PageIndex index ) const { return compare( key, keySize, this->page->key( index ), this->page->keySize( index ) ); };
    };
    template< class K > 
    class NodeCompareArray : public NodeCompare<K,true> {
        const K* key;
        const PageSize keySize;
        KeyCompare (*compare)( const K*, PageSize, const K*, PageSize );
    public:
        NodeCompareArray() = delete;
        NodeCompareArray( const Page<K,PageLink,true,false>* page, const K* k, const PageSize kS, KeyCompare (*compare)( const K*, PageSize, const K*, PageSize ) ) :
        NodeCompare<K,true>( page ), key( k ), keySize( kS ) { this->compare = compare; };
        KeyCompare operator()( PageIndex index ) const { return compare( key, keySize, this->page->key( index ), this->page->keySize( index ) ); };
    };

} // namespace BTree

#endif // BTREE_COMPARE_H