#ifndef STRING_2_STRING_BTREE_H
#define STRING_2_STRING_BTREE_H

#include "BTree.h"
#include <string.h>

namespace BTree {

    class String2StringTree : private Tree<char[],char[]> {
    public :
        String2StringTree(
            PagePool& pagePool,
            int (*compareKey)( const char*, PageIndex, const char*, PageIndex ) = defaultCompareArray<char>,
            UpdateMode updateMode = UpdateMode::Auto
        );
        bool insert( std::string key, std::string value );
        bool replace( std::string key, std::string value );
        std::string retrieve( std::string key ) const;
        void remove( std::string key );
        void commit();
        void recover();
    private:
        friend std::ostream & operator<<( std::ostream & o, const String2StringTree& tree );    
    };

}; // namespace BTree

#include "String2StringTree_Imp.h"

#endif // STRING_2_STRING_BTREE_H