#ifndef STRING_2_STRING_BTREE_IMP_H
#define STRING_2_STRING_BTREE_IMP_H

#include "String2StringTree.h"

namespace BTree {

    inline String2StringTree::String2StringTree(
        PagePool& pagePool,
        int (*compareKey)( const char*, PageIndex, const char*, PageIndex ),
        UpdateMode updateMode
    ) : Tree<char[],char[]>( pagePool, compareKey, updateMode )
    {}
    inline bool String2StringTree::insert( std::string key, std::string value ) {
        return Tree<char[],char[]>::insert( key.c_str(), static_cast<PageSize>(key.size()), value.c_str(), static_cast<PageSize>(value.size()) );
    }
    inline bool String2StringTree::replace( std::string key, std::string value ) {
        return Tree<char[],char[]>::replace( key.c_str(), static_cast<PageSize>(key.size()), value.c_str(), static_cast<PageSize>(value.size()) );
    }
    inline std::string String2StringTree::retrieve( std::string key ) const {
        std::pair<const char*, PageIndex> result = Tree<char[],char[]>::retrieve( key.c_str(), static_cast<PageSize>(key.size()) );
        return std::string( result.first, result.second );
    }
    inline void String2StringTree::erase( std::string key ) {
        Tree<char[],char[]>::erase( key.c_str(), static_cast<PageSize>(key.size()) );
    }
    inline void String2StringTree::commit() { Tree<char[],char[]>::commit(); }
    inline void String2StringTree::recover() { Tree<char[],char[]>::recover(); }

    std::ostream & operator<<( std::ostream & o, const String2StringTree& tree ) { tree.stream( o ); return o; }

}; // namespace BTree

#endif // STRING_2_STRING_BTREE_IMP_H