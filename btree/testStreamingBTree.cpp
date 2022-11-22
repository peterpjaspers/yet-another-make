#include "StreamingBTree.h"
#include "PersistentPagePool.h"

using namespace BTree;
using namespace std;

const int BTreePageSize = 512;

PagePool* createPagePool( bool persistent, string path, PageSize pageSize ) {
    if ( persistent ) {
        // Determine stored page size (if any)...
        PageSize storedPageSize = PersistentPagePool::pageCapacity( path );
        return new PersistentPagePool( ((0 < storedPageSize) ? storedPageSize : pageSize), path );
    } else {
        return new PagePool( pageSize );
    }
}

int main(int argc, char* argv[]) {
    BTree::StreamingTree<char,true> tree( *createPagePool( true, "StreamingBTree.bin", BTreePageSize ) );
};
