#ifndef STRING_2_VALUE_BTREE_H
#define STRING_2_VALUE_BTREE_H

#include "BTree.h"
#include <string.h>

// ToDo: Avoid static_cast<PageSize> by replacing with test on maximum size of array within a Page

namespace BTree {

    template< class V >
    class String2ValueTree : private Tree< char[], V > {
    public :
        template< class VT = V, std::enable_if_t<S<VT>,bool> = true >
        inline String2ValueTree(
            PagePool& pagePool,
            int (*compareKey)( const char*, PageIndex, const char*, PageIndex ) = defaultCompareArray<char>,
            UpdateMode updateMode = UpdateMode::Auto
        ) : Tree< char[], VT >::Tree( pagePool, compareKey, updateMode )
        {}
        template< class VT = V, std::enable_if_t<A<VT>,bool> = true >
        inline String2ValueTree(
            PagePool& pagePool,
            int (*compareKey)( const char*, PageIndex, const char*, PageIndex ) = defaultCompareArray<char>,
            UpdateMode updateMode = UpdateMode::Auto
        ) : Tree< char[], VT >::Tree( pagePool, compareKey, updateMode )
        {}
        template< class VT = V, std::enable_if_t<S<VT>,bool> = true >
        inline bool insert( std::string key, const VT& value ) {
            return Tree< char[], VT >::insert( key.c_str(), static_cast<PageSize>(key.size()), value );
        }
        template< class VT = V, std::enable_if_t<A<VT>,bool> = true >
        inline bool insert( std::string key, const B<VT>* value, PageSize valueSize ) {
            return Tree< char[], VT >::insert( key.c_str(), static_cast<PageSize>(key.size()), value, valueSize );
        }
        template< class VT = V, std::enable_if_t<S<VT>,bool> = true >
        inline bool replace( std::string key, const VT& value ) {
            return Tree< char[], VT >::replace( key.c_str(), static_cast<PageSize>(key.size()), value );
        }
        template< class VT = V, std::enable_if_t<A<VT>,bool> = true >
        inline bool replace( std::string key, const B<VT>* value, PageSize valueSize ) {
            return Tree< char[], VT >::replace( key.c_str(), static_cast<PageSize>(key.size()), value, valueSize );
        }
        template< class VT = V, std::enable_if_t<S<VT>,bool> = true >
        inline const B<VT>& at( std::string key ) const {
            return Tree< char[], VT >::at( key.c_str(), static_cast<PageSize>(key.size()) );
        }
        template< class VT = V, std::enable_if_t<A<VT>,bool> = true >
        inline std::pair<const B<VT>*, PageIndex> at( std::string key ) const {
            return Tree< char[], VT >::at( key.c_str(), static_cast<PageSize>(key.size()) );
        }
        template< class VT = V >
        inline void erase( std::string key ) {
            Tree< char[], VT >::erase( key.c_str(), key.size() );
        }
        template< class VT = V >
        inline void commit() { Tree< char[], VT >::commit(); }
        template< class VT = V >
        inline void recover() { Tree< char[], VT >::recover(); }
    private:
        template< class VT >
        friend std::ostream & operator<<( std::ostream & o, const String2ValueTree<VT>& tree );    
    };

    template< class VT >
    std::ostream & operator<<( std::ostream & o, const String2ValueTree<VT>& tree ) { tree.stream( o ); return o; }

}; // namespace BTree

#endif // STRING_2_VALUE_BTREE_H