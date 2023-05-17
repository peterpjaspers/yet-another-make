#ifndef STRING_2_STREAM_BTREE_IMP_H
#define STRING_2_STREAM_BTREE_IMP_H

// ToDo: (default) constructor arguments, specifically user defined key comparison

#include "String2StreamBTree.h"

namespace BTree {

    inline String2StreamTree::String2StreamTree( PagePool& indexPool, PagePool& valuePool, StreamBlockSize block, UpdateMode mode ) :
        StreamingTree<char[]>( indexPool, valuePool, block, mode )
    {}
    inline ValueWriter& String2StreamTree::insert( std::string key ) {
        return StreamingTree<char[]>::insert<char[]>( key.c_str(), key.size() );
    }
    inline ValueReader& String2StreamTree::retrieve( std::string key ) {
        return StreamingTree<char[]>::retrieve<char[]>( key.c_str(), key.size() );
    }
    inline void String2StreamTree::remove( std::string key ) {
        StreamingTree<char[]>::remove<char[]>( key.c_str(), key.size() );
    }
    inline void String2StreamTree::commit() { StreamingTree<char[]>::commit(); }
    inline void String2StreamTree::recover() { StreamingTree<char[]>::recover(); }

    inline std::ostream & operator<<( std::ostream & o, const String2StreamTree& tree ) { tree.stream( o ); return o; }

}; // namespace BTree

#endif // STRING_2_STREAM_BTREE_IMP_H