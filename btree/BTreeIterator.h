#ifndef BTREE_ITERATOR_H
#define BTREE_ITERATOR_H

#include "BTree.h"

namespace BTree {
    
    template< class K, class V, class T >
    class BTreeIterator : public std::iterator< std::bidirectional_iterator_tag, T, size_t, T*, T& > {
    private:
        Trail trail;
    public:
        BTreeIterator() = delete;
        BTreeIterator( const TreeBase& tree ) : trail( tree ) {}
        BTreeIterator( const BTreeIterator& iterator ) : trail( iterator.trail ) {}
        inline BTreeIterator& begin() { trail.begin<B<K>,A<K>>(); return *this; }
        inline BTreeIterator& end() { trail.end<B<K>,A<K>>(); return *this; }
        inline BTreeIterator& at( const Trail& position ) { trail = position; return *this; }
        inline BTreeIterator& operator++() { trail.next<B<K>,A<K>>(); return *this; }
        inline BTreeIterator operator++(int) { BTreeIterator tmp( *this ); ++(*this); return tmp; }
        inline BTreeIterator& operator--() { trail.previous<B<K>,A<K>>(); return *this; }
        inline BTreeIterator operator--(int) { BTreeIterator tmp( *this ); --(*this); return tmp; }
        inline bool operator==(BTreeIterator it) const { return( trail == it.trail ); }
        inline bool operator!=(BTreeIterator it) const { return( trail != it.trail ); }
        inline BTreeIterator& operator=(BTreeIterator& it) { trail = it.trail; return *this; }
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