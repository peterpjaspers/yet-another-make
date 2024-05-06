#ifndef VALUE_2_STRING_BTREE_H
#define VALUE_2_STRING_BTREE_H

#include "BTree.h"
#include <string.h>

namespace BTree {

    template< class K >
    class Value2StringTree : private Tree< K, char[] > {
    public :
        template< class KT = K >
        inline Value2StringTree(
            typename std::enable_if<S<KT>,PagePool>::type& pagePool,
            int(*compareKey)( const B<KT>&, const B<KT>& ) = defaultCompareScalar<KT>,
            UpdateMode updateMode = UpdateMode::Auto
        ) : Tree< KT, char[] >::Tree( pagePool, compareKey, updateMode )
        {}
        template< class KT = K >
        inline Value2StringTree(
            typename std::enable_if<A<KT>,PagePool>::type& pagePool,
           int(*compareKey)( const B<KT>*, PageIndex, const B<KT>*, PageIndex ) = defaultCompareArray<B<KT>>,
            UpdateMode updateMode = UpdateMode::Auto
        ) : Tree< KT, char[] >::Tree( pagePool, compareKey, updateMode )
        {}
        template< class KT = K, std::enable_if_t<S<KT>,bool> = true >
        inline bool insert( const KT& key, std::string value ) {
            return Tree< KT, char[] >::insert( key, value.c_str(), static_cast<PageSize>(value.size()) );
        }
        template< class KT = K, std::enable_if_t<A<KT>,bool> = true >
        inline bool insert( const B<KT>* key, PageSize keySize, std::string value ) {
            return Tree< KT, char[] >::insert( key, keySize, value.c_str(), static_cast<PageSize>(value.size()) );
        }
        template< class KT = K, std::enable_if_t<S<KT>,bool> = true >
        inline bool replace( const KT& key, std::string value ) {
            return Tree< KT, char[] >::replace( key, value.c_str(), static_cast<PageSize>(value.size()) );
        }
        template< class KT = K, std::enable_if_t<A<KT>,bool> = true >
        inline bool replace( const B<KT>* key, PageSize keySize, std::string value ) {
            return Tree< KT, char[] >::replace( key, keySize, value.c_str(), static_cast<PageSize>(value.size()) );
        }
        template< class KT = K >
        inline const typename std::enable_if<S<KT>,std::string>::type retrieve( const K& key ) const {
            std::pair< const char*, PageSize > result = Tree< KT, char[] >::retrieve( key );
            return std::string( result.first, result.second );
        }
        template< class KT = K >
        inline const typename std::enable_if<A<KT>,std::string>::type retrieve( const B<KT>* key, PageSize keySize ) const {
            std::pair< const char*, PageSize > result = Tree< KT, char[] >::retrieve( key, keySize );
            return std::string( result.first, result.second );
        }
        template< class KT = K >
        inline typename std::enable_if<S<KT>,void>::type erase( const K& key ) {
            Tree< KT, char[] >::erase( key );
        }
        template< class KT = K >
        inline typename std::enable_if<A<KT>,void>::type erase( const K* key, PageSize keySize ) {
            Tree< KT, char[] >::erase( key, keySize );
        }
        inline void commit() { Tree< K, char[] >::commit(); }
        inline void recover() { Tree< K, char[] >::recover(); }
    private:
        template< class KT >
        friend std::ostream & operator<<( std::ostream & o, const Value2StringTree<KT>& tree );    
    };

    template< class KT >
    std::ostream & operator<<( std::ostream & o, const Value2StringTree<KT>& tree ) { tree.stream( o ); return o; }

}; // namespace BTree

#endif // VALUE_2_STRING_BTREE_H