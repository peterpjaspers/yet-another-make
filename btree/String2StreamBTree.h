#ifndef STRING_2_STREAM_BTREE_H
#define STRING_2_STREAM_BTREE_H

#include "StreamingBTree.h"
#include <string.h>

namespace BTree {

    // B-Tree mapping std::string keys to stream values.
    // See StreamingTree for streaming details.
    class String2StreamTree : private StreamingTree< char[] > {
    public :
        // Construct a string to stream B-Tree on the given page pool with the given stream block size.
        String2StreamTree( PagePool& indexPool, PagePool& valuePool, StreamBlockSize block, UpdateMode mode = Auto );
        // Insert a value in the B-Tree.
        ValueWriter& insert( std::string key );
        // Retrieve a value from the B-Tree.
        ValueReader& retrieve( std::string key );
        // Remove a value from the B-Tree.
        void remove( std::string key );
        // Commit updates to the B-Tree.
        void commit();
        // Recover B-Tree to previous commit state.
        void recover();
    private:
        // Stream B-Tree content in human readable format.
        friend std::ostream & operator<<( std::ostream & o, const String2StreamTree& tree );
    };

}; // namespace BTree

#include "String2StreamBTree_Imp.h"

#endif // STRING_2_STREAM_BTREE_H