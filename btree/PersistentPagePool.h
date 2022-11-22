#ifndef PERSISTENT_PAGE_POOL_H
#define PERSISTENT_PAGE_POOL_H

#include "PagePool.h"

namespace BTree {

    class PersistentPagePool : public PagePool {
    private:
        // Pages to be recoverd from persistent store when a transaction fails (call to recover()).
        // Note that pages in this list may also reside in the modified list and/or the free list
        // depending on the operations performed on the page. The recover list is simply emptied
        // when a transaction succeeds.
        std::vector<PageLink> recoverPages;
        std::string fileName;
    public:
        PersistentPagePool( PageSize pageCapacity, const std::string path );
        bool persistent() const { return true; };
        void recover( const PageHeader& page, bool reuse = false );
        void commit( const PageLink link );
        PageLink recover( bool freeModifiedPages );
        static PageSize pageCapacity( const std::string path );
    };

} // namespace BTree

#endif // PERSISTENT_PAGE_POOL_H
