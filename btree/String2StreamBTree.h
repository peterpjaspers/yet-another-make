#ifndef STRING_2_STREAM_BTREE_H
#define STRING_2_STREAM_BTREE_H

// ToDo: (default) constructor arguments, specifically user defined key comparison

#include "StreamingBTree.h"

namespace BTree {

    class String2StreamTree : private StreamingTree< char, true > {
    public :
        inline String2StreamTree( PagePool& pool, StreamBlockSize block, UpdateMode mode = Auto ) :
            StreamingTree<char,true>( pool, block, mode )
        {}
        inline ValueWriter& insert( std::string key ) {
            return StreamingTree<char,true>::insert( key.c_str(), key.size() );
        }
        inline ValueReader& retrieve( std::string key ) {
            return StreamingTree<char,true>::retrieve( key.c_str(), key.size() );
        }
        inline void remove( std::string key ) {
            StreamingTree<char,true>::remove( key.c_str(), key.size() );
        }
        inline void commit() { StreamingTree<char,true>::commit(); }
        inline void recover() { StreamingTree<char,true>::recover(); }
    private:
        friend std::ostream & operator<<( std::ostream & o, const String2StreamTree& tree );
    };

    inline std::ostream & operator<<( std::ostream & o, const String2StreamTree& tree ) { tree.stream( o ); return o; }

}; // namespace BTree

#endif // STRING_2_STREAM_BTREE_H