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
    class StreamingTreeIterator : public TreeIterator<StreamKey<K>,uint8_t[],std::pair<const StreamKey<K>&,std::pair<const uint8_t*,PageSize>>> {
        Trail trailBegin;
        Trail trailEnd;
    public:
        StreamingTreeIterator() = delete;
        StreamingTreeIterator( const StreamingTree<K>& streamingTree ) :
            TreeIterator<StreamKey<K>,uint8_t[],std::pair<const StreamKey<K>&,std::pair<const uint8_t*,PageSize>>>( streamingTree ),
            trailBegin( tree() ),
            trailEnd( tree() )
        {
            trailBegin.begin<K,false>();
            trailEnd.end<K,false>();
        }
        StreamingTreeIterator( const StreamingTreeIterator& iterator ) :
            TreeIterator<StreamKey<K>,uint8_t[],std::pair<const StreamKey<K>&,std::pair<const uint8_t*,PageSize>>>( iterator ),
            trailBegin( tree() ),
            trailEnd( tree() )
        {
            trailBegin.begin<K,false>();
            trailEnd.end<K,false>();
        }
        inline StreamingTreeIterator& begin() {
            TreeIterator<StreamKey<K>,uint8_t[],std::pair<const StreamKey<K>&,std::pair<const uint8_t*,PageSize>>>::begin(); return *this;
        }
        inline StreamingTreeIterator& end() {
            TreeIterator<StreamKey<K>,uint8_t[],std::pair<const StreamKey<K>&,std::pair<const uint8_t*,PageSize>>>::end(); return *this;
        }
        inline StreamingTreeIterator& at( const Trail& position ) {
            TreeIterator<StreamKey<K>,uint8_t[],std::pair<const StreamKey<K>&,std::pair<const uint8_t*,PageSize>>>::at( position ); return *this;
        }
        inline StreamingTreeIterator& operator++() {
            auto currentKey = key();
            TreeIterator<StreamKey<K>,uint8_t[],std::pair<const StreamKey<K>&,std::pair<const uint8_t*,PageSize>>>::operator++();
            while ((TreeIterator<StreamKey<K>,uint8_t[],std::pair<const StreamKey<K>&,std::pair<const uint8_t*,PageSize>>>::trail != trailEnd) && (currentKey == key())) {
                TreeIterator<StreamKey<K>,uint8_t[],std::pair<const StreamKey<K>&,std::pair<const uint8_t*,PageSize>>>::operator++();
            }
            return *this;
        }
        inline StreamingTreeIterator operator++(int) { StreamingTreeIterator tmp( *this ); ++(*this); return tmp; }
        inline StreamingTreeIterator& operator--() {
            auto currentKey = key();
            TreeIterator<StreamKey<K>,uint8_t[],std::pair<const StreamKey<K>&,std::pair<const uint8_t*,PageSize>>>::operator--();
            while ((TreeIterator<StreamKey<K>,uint8_t[],std::pair<const StreamKey<K>&,std::pair<const uint8_t*,PageSize>>>::trail != trailBegin) && (currentKey == key())) {
                TreeIterator<StreamKey<K>,uint8_t[],std::pair<const StreamKey<K>&,std::pair<const uint8_t*,PageSize>>>::operator--();
            }
            return *this;
        }
        inline StreamingTreeIterator operator--(int) { StreamingTreeIterator tmp( *this ); --(*this); return tmp; }
        ValueReader<K>& operator*() const { return value(); }

        const StreamingTree<K>& tree() const {
            const TreeBase& sourceTree = TreeIterator<StreamKey<K>,uint8_t[],std::pair<const StreamKey<K>&,std::pair<const uint8_t*,PageSize>>>::tree();
            return reinterpret_cast<const StreamingTree<K>&>( sourceTree );
        }
        // Retrieve key at current position
        const K& key() const {
            const StreamKey<K>& streamKey = TreeIterator<StreamKey<K>,uint8_t[],std::pair<const StreamKey<K>&,std::pair<const uint8_t*,PageSize>>>::key();
            return streamKey.key;
        }
        // Retrieve value at current iterator position.
        // Returns an opened ValueReader for the stream at the current posisiton.
        ValueReader<K>& value() const {
            return tree().retrieve( key() );
        }

    };

}; // namespace BTree

#endif // STREAMING_BTREE_ITERATOR