#ifndef STREAMING_BTREE_H
#define STREAMING_BTREE_H

#include "BTree.h"
#include "ValueStreamer.h"

// ToDo: Reuse indeces by maintaining a collection of deleted indeces...
// ToDo: Add constructor argument to enable user defined key comparison

namespace BTree {

    int compareIndexRange( const StreamKey& a, const StreamKey& b ) {
        if ( std::less<StreamIndex>()( a.index, b.index ) ) return( -1 );
        if ( std::less<StreamIndex>()( b.index, a.index ) ) return( +1 );
        if ( std::less<StreamSequence>()( a.sequence, b.sequence ) ) return( -1 );
        if ( std::less<StreamSequence>()( b.sequence, a.sequence ) ) return( +1 );
        return( 0 );
    };

    template< class K, bool KA >
    class StreamingTree {
    private:
        Tree<K,StreamIndex,KA,false> indeces;
        Tree<StreamKey,uint8_t,false,true> values;
        StreamBlockSize blockSize;
        StreamIndex index;
        template< class SK, bool SKA >
        friend std::ostream & operator<<( std::ostream & o, const StreamingTree<SK, SKA>& tree );
    public:
        StreamingTree( PagePool& pool, StreamBlockSize block, UpdateMode mode = Auto ) :
            indeces( Tree<K,StreamIndex,KA,false>( pool, mode, 0  ) ),
            values( Tree<StreamKey,uint8_t,false,true>( pool, mode, nullptr, UINT16_MAX, compareIndexRange ) ),
            blockSize( block ),
            index( 1 )
        {
            static const std::string signature( "StreamingTree( PagePool& pool, StreamBlockSize block, UpdateMode mode )" );
            if ( (pool.pageCapacity() / 8) < block ) throw signature + " - Invalid block size";
        };

        template<bool AK = KA>
        typename std::enable_if<(!AK),ValueWriter>::type& insert( const K& key ) {
            StreamIndex index = indeces.retrieve( key );
            if ( index != 0 ) {
                removeBlocks( index );
            } else {
                index = StreamingTree::index++;
                indeces.insert( key, index );
            }
            return *( new ValueWriter( values, index, blockSize ) );
        }
        template<bool AK = KA>
        typename std::enable_if<(!AK),ValueReader>::type& retrieve( const K& key ) {
            return *( new ValueReader( values, indeces.retrieve( key ) ) );
        }
        template<bool AK = KA>
        typename std::enable_if<(!AK),void>::type remove( const K& key ) {
            removeBlocks( indeces.retrieve( key ) );
        }
        template<bool AK = KA>
        typename std::enable_if<(AK),ValueWriter>::type& insert( const K* key, PageSize keySize ) {
            StreamIndex index = indeces.retrieve( key, keySize );
            if ( index != 0 ) {
                removeBlocks( index );
            } else {
                index = StreamingTree::index++;
                indeces.insert( key, keySize, index );
            }
            return *( new ValueWriter( values, index, blockSize ) );
        }
        template<bool AK = KA>
        typename std::enable_if<(AK),ValueReader>::type& retrieve( const K* key, PageSize keySize ) {
            return *( new ValueReader( values, indeces.retrieve( key, keySize ) ) );
        }
        template<bool AK = KA>
        typename std::enable_if<(AK),void>::type remove( const K*key, PageSize keySize ) {
            removeBlocks( indeces.retrieve( key, keySize ) );
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

    template< class K, bool KA >
    inline std::ostream & operator<<( std::ostream & o, const StreamingTree<K,KA>& tree ) { tree.stream( o ); return o; }

}; // namespace BTree

#endif // STREAMING_BTREE_H