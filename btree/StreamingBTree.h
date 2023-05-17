#ifndef STREAMING_BTREE_H
#define STREAMING_BTREE_H

#include "BTree.h"
#include "ValueStreamer.h"

// ToDo: Reuse indeces by maintaining a collection of deleted indeces...
// ToDo: Add constructor argument to enable user defined key comparison...
// ToDo: Implement B-Tree of B-Trees to enable two B-Trees to reside in a single page pool.
// ToDo: Iterator over streaming B-Tree to return streamers.

namespace BTree {

    int compareIndexRange( const StreamKey& a, const StreamKey& b ) {
        if ( std::less<StreamIndex>()( a.index, b.index ) ) return( -1 );
        if ( std::less<StreamIndex>()( b.index, a.index ) ) return( +1 );
        if ( std::less<StreamSequence>()( a.sequence, b.sequence ) ) return( -1 );
        if ( std::less<StreamSequence>()( b.sequence, a.sequence ) ) return( +1 );
        return( 0 );
    };

    template< class K >
    class StreamingTree {
    private:
        Tree<K,StreamIndex> indeces;
        Tree<StreamKey,uint8_t[]> values;
        ValueReader reader;
        ValueWriter writer;
        StreamBlockSize blockSize;
        StreamIndex index;
        template< class KT >
        friend std::ostream & operator<<( std::ostream & o, const StreamingTree<KT>& tree );
    public:
        StreamingTree( PagePool& indexPool, PagePool& valuePool, StreamBlockSize block, UpdateMode mode = Auto ) :
            indeces( indexPool, mode, 0 ),
            values( valuePool, mode, nullptr, UINT16_MAX, compareIndexRange ),
            reader( values ),
            writer( values, block ),
            index( 1 )
        {
            static const char* signature( "StreamingTree( PagePool& indexPool, PagePool& valuePool, StreamBlockSize block, UpdateMode mode )" );
            if ( (valuePool.pageCapacity() / 8) < block ) throw std::string( signature ) + " - Invalid block size";
        };

        template< class KT = K, std::enable_if_t<(S<KT>),bool> = true >
        ValueWriter& insert( const KT& key ) {
            StreamIndex index = indeces.retrieve( key );
            if ( index != 0 ) {
                removeBlocks( index );
            } else {
                index = StreamingTree::index++;
                indeces.insert( key, index );
            }
            return writer.open( index );
        }
        template< class KT = K, std::enable_if_t<(S<KT>),bool> = true >
        ValueReader& retrieve( const KT& key ) const {
            return reader.open( indeces.retrieve( key ) );
        }
        template< class KT = K, std::enable_if_t<(S<KT>),bool> = true >
        void remove( const KT& key ) {
            removeBlocks( indeces.retrieve( key ) );
            indeces.remove( key );
        }
        template< class KT = K, std::enable_if_t<(A<KT>),bool> = true >
        ValueWriter& insert( const B<KT>* key, PageSize keySize ) {
            StreamIndex index = indeces.retrieve( key, keySize );
            if ( index != 0 ) {
                removeBlocks( index );
            } else {
                index = StreamingTree::index++;
                indeces.insert( key, keySize, index );
            }
            return writer.open( index );
        }
        template< class KT = K, std::enable_if_t<(A<KT>),bool> = true >
        ValueReader& retrieve( const B<KT>* key, PageSize keySize ) {
            return reader.open( indeces.retrieve( key, keySize ) );
        }
        template< class KT = K, std::enable_if_t<(A<KT>),bool> = true >
        void remove( const B<KT>*key, PageSize keySize ) {
            removeBlocks( indeces.retrieve( key, keySize ) );
            indeces.remove( key, keySize );
        }
        void commit() { indeces.commit(); values.commit(); };
        void recover(){ indeces.recover(); values.recover(); };
    private:
        // Remove all blocks associated with an index...
        void removeBlocks( StreamIndex index ) {
            StreamKey key( index, 0 );
            while (values.remove( key )) key.sequence += 1;
        }
    protected:
        void stream( std::ostream & o ) const {
            o << "Streaming BTree [ " << blockSize << "]\n";
            o << "Indeces : " << indeces;
            o << "Values : " << values;
        }
    }; // class StreamingTree

    template< class K >
    inline std::ostream & operator<<( std::ostream & o, const StreamingTree<K>& tree ) { tree.stream( o ); return o; }

}; // namespace BTree

#endif // STREAMING_BTREE_H