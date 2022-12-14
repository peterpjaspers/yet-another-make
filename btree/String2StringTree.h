#ifndef STRING_2_STRING_BTREE_H
#define STRING_2_STRING_BTREE_H

#include "BTree.h"
#include <string.h>

namespace BTree {

    class String2StringTree : private Tree<char,char,true,true> {
    public :
        inline String2StringTree(
            PagePool& pagePool,
            UpdateMode updateMode = UpdateMode::Auto,
            const char* defaultValue = nullptr,
            PageIndex defaultValueSize = 0,
            int (*compareKey)( const char*, PageIndex, const char*, PageIndex ) = defaultCompareArray<char>
        ) : Tree<char,char,true,true>( pagePool, updateMode, defaultValue, defaultValueSize )
        {}
        inline void insert( std::string key, std::string value ) {
            return Tree<char,char,true,true>::insert( key.c_str(), key.size(), value.c_str(), value.size() );
        }
        inline std::string retrieve( std::string key ) {
            std::pair<const char*, PageIndex> result = Tree<char,char,true,true>::retrieve( key.c_str(), key.size() );
            return std::string( result.first, result.second );
        }
        inline void remove( std::string key ) {
            Tree<char,char,true,true>::remove( key.c_str(), key.size() );
        }
        inline void commit() { Tree<char,char,true,true>::commit(); }
        inline void recover() { Tree<char,char,true,true>::recover(); }
    private:
        friend std::ostream & operator<<( std::ostream & o, const String2StringTree& tree );    
    };

    std::ostream & operator<<( std::ostream & o, const String2StringTree& tree ) { tree.stream( o ); return o; }

}; // namespace BTree

#endif // STRING_2_STRING_BTREE_H