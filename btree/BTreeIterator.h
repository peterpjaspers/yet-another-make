#ifndef BTREE_ITERATOR_H
#define BTREE_ITERATOR_H

#include "BTree.h"

#include <iterator>

// ToDo: Provide const TreeIterator (ConstTreeIterator). begin, end and find functions on Tree are const.
// ToDo: provide assignment (and operator)
// ToDo: avoid dynamic allocation of iterators in Tree::begin,end & find

namespace BTree {
    
    // B-directional iterator on B-Trees.
    //
    // Template arguments:
    //
    //  K   class of key objects
    //  V   class of vlaue objects
    //  T   class of key-value pair pointed to by iterator
    //
    // In addition to standard bi-directional iterator operations, it provides
    // key() and value() to retrieve the key and value at particular iterator position.
    //
    template< class K, class V, class T >
    class TreeIterator : public std::iterator< std::bidirectional_iterator_tag, T, size_t, T*, T& > {
    protected:
        Trail trail;
    public:
        // Implement these typedefs to avoid deriving from class iterator (deprecated since C++17)
        /*
        typedef difference_type nullptr_t;
        typedef value_type std::remove_cv_t<T>(T);
        typedef pointer std::pair< const B<K>&, const V& >; // typedef depends on B and S on K and V
        typedef reference std::pair< const B<K>&, const V& >; // typedef depends on B and S on K and V
        typedef iterator_category std::bidirectional_iterator_tag;
        */
        TreeIterator() = delete;
        TreeIterator( const TreeBase& tree ) : trail( tree ) {}
        TreeIterator( const TreeIterator& iterator ) : trail( iterator.trail ) {}
        inline TreeIterator& begin() { trail.begin<B<K>,A<K>>(); return *this; }
        inline TreeIterator& end() { trail.end<B<K>,A<K>>(); return *this; }
        inline TreeIterator& position( const Trail& pos ) { trail = pos; return *this; }
        inline TreeIterator& operator++() { trail.next<B<K>,A<K>>(); return *this; }
        inline TreeIterator operator++(int) { TreeIterator tmp( *this ); ++(*this); return tmp; }
        inline TreeIterator& operator--() { trail.previous<B<K>,A<K>>(); return *this; }
        inline TreeIterator operator--(int) { TreeIterator tmp( *this ); --(*this); return tmp; }
        inline bool operator==(TreeIterator it) const { return( trail == it.trail ); }
        inline bool operator!=(TreeIterator it) const { return( trail != it.trail ); }
        inline TreeIterator& operator=(TreeIterator& it) { trail = it.trail; return *this; }
        template< class KT = K, class VT = V, std::enable_if_t<(S<KT>&&S<VT>),bool> = true >
        std::pair< const B<KT>&, const VT& > operator*() const {
            return{  key<KT>(), value<VT>() };
        }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>&&S<VT>),bool> = true >
        std::pair< std::pair< const B<KT>*, PageSize >, const B<VT>& > operator*() const {
            return{ key<KT>(), value<VT>() };
        }
        template< class KT = K, class VT = V, std::enable_if_t<(S<KT>&&A<VT>),bool> = true >
        std::pair< const B<KT>&, std::pair< const B<VT>*, PageSize > > operator*() const {
            return{ key<KT>(), value<VT>() };
        }
        template< class KT = K, class VT = V, std::enable_if_t<(A<KT>&&A<VT>),bool> = true >
        std::pair< std::pair< const B<KT>*, PageSize >, std::pair< const B<VT>*, PageSize > > operator*() const {
            return{ key<KT>(), value<VT>() };
        }
        // Return the B-Tree being iterated.
        const TreeBase& tree() const { return trail.sourceTree(); }
        template< class KT = K, std::enable_if_t<(S<KT>),bool> = true >
        const B<KT>& key() const {
            if (trail.atSplit()) {
                PageDepth offset = trail.match();
                const auto page = trail.page<B<KT>,PageLink,false,false>( offset );
                return page->key( trail.index( offset ) );
            }
            const auto page = trail.page<B<KT>,B<V>,false,A<V>>();
            return page->key( trail.index() );
        }
        // Retrieve key at current iterator position.
        template< class KT = K, std::enable_if_t<(A<KT>),bool> = true >
        std::pair< const B<KT>*, PageSize > key() const {
            if (trail.atSplit()) {
                PageDepth offset = trail.match();
                const auto page = trail.page<B<KT>,PageLink,true,false>( offset );
                return{ page->key( trail.index( offset ) ), page->keySize( trail.index( offset ) ) };
            }
            const auto page = trail.page<B<KT>,B<V>,true,A<V>>();
            return{ page->key( trail.index() ), page->keySize( trail.index() ) };
        }
        // Retrieve value at current iterator position.
        template< class VT = V, std::enable_if_t<(S<VT>),bool> = true >
        const B<VT>& value() const {
            const auto page = trail.page<B<K>,B<VT>,A<K>,false>();
            if (trail.atSplit()) return page->split();
            return page->value( trail.index() );
        }
        template< class VT = V, std::enable_if_t<(A<VT>),bool> = true >
        std::pair< const B<VT>*, PageSize > value() const {
            const auto page = trail.page<B<K>,B<VT>,A<K>,true>();
            if (trail.atSplit()) return{ page->split(), page->splitSize() };
            return{ page->value( trail.index() ), page->valueSize( trail.index() ) };
        }
    };

}; // namespace BTree

#endif // BTREE_ITERATOR_H