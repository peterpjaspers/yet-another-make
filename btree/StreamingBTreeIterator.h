#ifndef STREAMING_BTREE_ITERATOR
#define STREAMING_BTREE_ITERATOR

#include "BTreeIterator.h"
#include "StreamingBTree.h"

namespace BTree {
        
    // B-directional iterator on streaming B-Trees.
    //
    // Template arguments:
    //
    //  K   class of key objects
    //
    template< class K >
    class StreamingTreeIterator : public TreeIterator<StreamKey<K>,ValueReader<K>,std::pair<const K&,ValueReader<K>&>> {
        Trail trailBegin;
        Trail trailEnd;
    public:
        StreamingTreeIterator() = delete;
        StreamingTreeIterator( const StreamingTree<K>& streamingTree ) :
            TreeIterator<StreamKey<K>,ValueReader<K>,std::pair<const K&,ValueReader<K>&>>( streamingTree ),
            trailBegin( tree() ),
            trailEnd( tree() )
        {
            trailBegin.begin<K,false>();
            trailEnd.end<K,false>();
        }
        StreamingTreeIterator( const StreamingTreeIterator& iterator ) :
            TreeIterator<StreamKey<K>,ValueReader<K>,std::pair<const K&,ValueReader<K>&>>( iterator ),
            trailBegin( tree() ),
            trailEnd( tree() )
        {
            trailBegin.begin<K,false>();
            trailEnd.end<K,false>();
        }
        inline StreamingTreeIterator& begin() { TreeIterator<StreamKey<K>,ValueReader<K>,std::pair<const K&,ValueReader<K>&>>::begin(); return *this; }
        inline StreamingTreeIterator& end() { TreeIterator<StreamKey<K>,ValueReader<K>,std::pair<const K&,ValueReader<K>&>>::end(); return *this; }
        inline StreamingTreeIterator& at( const Trail& position ) { TreeIterator<StreamKey<K>,ValueReader<K>,std::pair<const K&,ValueReader<K>&>>::at( position ); return *this; }
        inline StreamingTreeIterator& operator++() {
            auto currentKey = key();
            TreeIterator<StreamKey<K>,ValueReader<K>,std::pair<const K&,ValueReader<K>&>>::operator++();
            while ((TreeIterator<StreamKey<K>,ValueReader<K>,std::pair<const K&,ValueReader<K>&>>::trail != trailEnd) && (currentKey == key())) TreeIterator<StreamKey<K>,ValueReader<K>,std::pair<const K&,ValueReader<K>&>>::operator++();
            return *this;
        }
        inline StreamingTreeIterator operator++(int) { StreamingTreeIterator tmp( *this ); ++(*this); return tmp; }
        inline StreamingTreeIterator& operator--() {
            auto currentKey = key();
            TreeIterator<StreamKey<K>,ValueReader<K>,std::pair<const K&,ValueReader<K>&>>::operator--();
            while ((TreeIterator<StreamKey<K>,ValueReader<K>,std::pair<const K&,ValueReader<K>&>>::trail != trailBegin) && (currentKey == key())) TreeIterator<StreamKey<K>,ValueReader<K>,std::pair<const K&,ValueReader<K>&>>::operator--();
            return *this;
        }
        inline StreamingTreeIterator operator--(int) { StreamingTreeIterator tmp( *this ); --(*this); return tmp; }
        std::pair< const K&, ValueReader<K>& > operator*() const {
            return{  key(), value() };
        }

        const Tree<StreamKey<K>,uint8_t[]>& tree() const {
            const TreeBase& sourceTree = TreeIterator<StreamKey<K>,ValueReader<K>,std::pair<const K&,ValueReader<K>&>>::tree();
            return reinterpret_cast<const Tree<StreamKey<K>,uint8_t[]>&>( sourceTree );
        }
        // Retrieve key at current position
        const K& key() const {
            const StreamKey<K>& streamKey = TreeIterator<StreamKey<K>,ValueReader<K>,std::pair<const K&,ValueReader<K>&>>::key();
            return streamKey.key;
        }
        // Retrieve value at current itertor position.
        // Returns an opened ValueReader for the stream at the current posisiton.
        ValueReader<K>& value() const {
            return ValueReader<K>( tree() ).open( key() );
        }


    };

}; // namespace BTree

#endif // STREAMING_BTREE_ITERATOR