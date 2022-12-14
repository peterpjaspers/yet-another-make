#ifndef STRING_2_VALUE_BTREE_H
#define STRING_2_VALUE_BTREE_H

#include "BTree.h"
#include <string.h>

namespace BTree {

    template< class V, bool VA >
    class String2ValueTree : private Tree< char, V, true, VA > {
    public :
        template<bool AV = VA>
        inline String2ValueTree(
            typename std::enable_if<(!AV),PagePool>::type& pagePool,
            UpdateMode updateMode = UpdateMode::Auto,
            const V& defaultValue = V(),
            int (*compareKey)( const char*, PageIndex, const char*, PageIndex ) = defaultCompareArray<char>
        ) : Tree< char, V, true, false >::Tree( pagePool, updateMode, defaultValue )
        {}
        template<bool AV = VA>
        inline String2ValueTree(
            typename std::enable_if<(AV),PagePool>::type& pagePool,
            UpdateMode updateMode = UpdateMode::Auto,
            const V* defaultValue = nullptr,
            PageIndex defaultValueSize = 0,
            int (*compareKey)( const char*, PageIndex, const char*, PageIndex ) = defaultCompareArray<char>
        ) : Tree< char, V, true, true >( pagePool, updateMode, defaultValue, defaultValueSize )
        {}
        template< bool AV = VA >
        inline typename std::enable_if<(!AV),void>::type insert( std::string key, const V& value ) {
            return Tree< char, V, true, false >::insert( key.c_str(), key.size(), value );
        }
        template< bool AV = VA >
        inline typename std::enable_if<(AV),void>::type insert( std::string key, const V* value, PageSize valueSize ) {
            return Tree< char, V, true, true >::insert( key.c_str(), key.size(), value );
        }
        template< bool AV = VA >
        inline const typename std::enable_if<(!AV),V>::type& retrieve( std::string key ) {
            return Tree< char, V, true, false >::retrieve( key.c_str(), key.size() );
        }
        template< bool AV = VA >
        inline typename std::enable_if<(AV),std::pair<const V*, PageIndex>>::type retrieve( std::string key ) {
            return Tree< char, V, true, true >::retrieve( key.c_str(), key.size() );
        }
        inline void remove( std::string key ) {
            Tree< char, V, true, VA >::remove( key.c_str(), key.size() );
        }
        inline void commit() { Tree<char,V,true,VA>::commit(); }
        inline void recover() { Tree<char,V,true,VA>::recover(); }
    private:
        template< class VT, bool AV >
        friend std::ostream & operator<<( std::ostream & o, const String2ValueTree<VT,AV>& tree );    
    };

    template< class VT, bool AV >
    std::ostream & operator<<( std::ostream & o, const String2ValueTree<VT,AV>& tree ) { tree.stream( o ); return o; }

}; // namespace BTree

#endif // STRING_2_VALUE_BTREE_H