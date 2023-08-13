#ifndef STREAMING_BTREE_H
#define STREAMING_BTREE_H

#include "BTree.h"
#include "ValueStreamer.h"

namespace BTree {

    template< class K > class StreamingTreeIterator;

    template< class K >
    int compareStreamKey( const StreamKey<K>& a, const StreamKey<K>& b ) {
        if ( std::less<K>()( a.key, b.key ) ) return( -1 );
        if ( std::less<K>()( b.key, a.key ) ) return( +1 );
        if ( std::less<StreamSequence>()( a.sequence, b.sequence ) ) return( -1 );
        if ( std::less<StreamSequence>()( b.sequence, a.sequence ) ) return( +1 );
        return( 0 );
    };

    template< class K >
    class StreamingTree : public Tree<StreamKey<K>,uint8_t[]> {
    private:
        mutable ValueReader<K> reader;
        mutable ValueWriter<K> writer;
        template< class KT >
        friend std::ostream & operator<<( std::ostream & o, const StreamingTree<KT>& tree );
    public:
        StreamingTree( PagePool& pool, UpdateMode mode = Auto ) :
            Tree<StreamKey<K>,uint8_t[]>( pool, compareStreamKey<K>, mode ),
            reader( *this ),
            writer( *this, Page<StreamKey<K>,uint8_t,false,true>::optimalBlockSize( pool.pageCapacity() ) )
        {};

        // Normal B-Tree modifiers and access disabled for StreamingTree...
        bool insert( const K& key, const uint8_t* value ) = delete;
        bool replace( const K& key, const uint8_t* value ) = delete;
        StreamingTreeIterator<K> begin() const {
            auto it = new StreamingTreeIterator<K>( *this );
            it->begin();
            return *it;
        };
        StreamingTreeIterator<K> end() const {
            auto it = new StreamingTreeIterator<K>( *this );
            it->end();
            return *it;
        };
        StreamingTreeIterator<K> find( const K& key ) const {
            StreamKey<K> streamKey( key, 0 );
            Trail trail( *this );
            bool found = Tree<StreamKey<K>,uint8_t[]>::lookUp( streamKey, trail );
            auto it = new StreamingTreeIterator<K>( *this );
            if (found) return it.iterator->find( trail );
            return it.iterator->end();
        };
        void assign( const Tree<StreamKey<K>,uint8_t[]>& tree ) = delete;

        bool contains( const K& key) const {
            StreamKey<K> streamKey( key, 0 );
            return Tree<StreamKey<K>,uint8_t[]>::contains( streamKey );
        }

        // Insert a streamed object.
        // Returns writer streamer to stream the object.
        ValueWriter<K>& insert( const K& key ) {
            static const char* signature = "ValueWriter<K>& StreamingTree<K>::insert( const K& key )";
            if (reader.isOpen() && (reader.key() == key)) {
                throw std::string( signature ) + " : Accessing writer stream on open reader stream";
            }
            StreamKey<K> streamKey( key, 0 );
            if ( Tree<StreamKey<K>,uint8_t[]>::contains( streamKey ) ) removeBlocks( key );
            return writer.open( key );
        }
        // Retrieve a streamed object.
        // Returns value reader streamer to stream the object.
        ValueReader<K>& retrieve( const K& key ) const {
            static const char* signature = "ValueReader<K>& StreamingTree<K>::retrieve( const K& key )";
            if (writer.isOpen() && (writer.key() == key)) {
                throw std::string( signature ) + " : Accessing reader stream on open writer stream";
            }
            if (reader.isOpen()) {
                throw std::string( signature ) + " : Accessing open reader stream";
            }
            reader.open( key );
            return reader;
        }
        // Remove a streamed object.
        bool erase( const K& key ) {
            StreamKey<K> streamKey( key, 0 );
            if ( Tree<StreamKey<K>,uint8_t[]>::contains( streamKey ) ) {
                removeBlocks( key );
                return true;
            }
            return false;
        }

    private:
        // Remove all blocks associated with a key...
        void removeBlocks( const K& key ) {
            StreamKey<K> streamKey( key, 0 );
            while ( Tree<StreamKey<K>,uint8_t[]>::erase( streamKey ) ) streamKey.sequence += 1;
        }
    protected:
        void stream( std::ostream & o ) const {
            o << "Streaming BTree\n";
            Tree<StreamKey<K>,uint8_t[]>::stream( o );
        }
    }; // template< class K > class StreamingTree

    template< class K >
    inline std::ostream & operator<<( std::ostream & o, const StreamingTree<K>& tree ) { tree.stream( o ); return o; }

}; // namespace BTree

#include "StreamingBTreeIterator.h"

#endif // STREAMING_BTREE_H