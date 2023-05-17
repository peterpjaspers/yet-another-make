#ifndef PERSISTENT_PAGE_POOL_H
#define PERSISTENT_PAGE_POOL_H

#include "PagePool.h"

namespace BTree {

    // A PersistentPagePool maintains a PagePool on a persistent backing store file.
    // Pages are written to the file when a transaction succeeds (commit).
    // Pages are read from the file when a transaction fails (recover).
    // As all references between pages are via PageLink values
    // (i.e., no memory addresses are used), the external representation of
    // a page is identical to its memory representation and no transformation
    // (for example in the form of streaming) is required when accessing the
    // persistent file.
    class PersistentPagePool : public PagePool {
    private:
        // Pages to be recoverd from persistent store when a transaction fails (call to recover()).
        // Note that pages in this list may also reside in the modified list and/or the free list
        // depending on the operations performed on the page. The recover list is simply emptied
        // when a transaction succeeds.
        std::vector<PageLink> recoverPages;
        std::string fileName;
    public:
        // Create a persistent page pool on the file path with pages of the given capacity.
        PersistentPagePool( PageSize pageCapacity, const std::string path );
        // Destroy the persistent page pool, updating the file to a consistent state.
        ~PersistentPagePool();
        // Condition indicating that this page pool is persistent.
        // Used to influence page update mode.
        inline bool persistent() const { return true; };
        // Mark a page as pending recovery.
        // Frees the page if reuse is enabled.
        void recover( const PageHeader& page, bool reuse = false );
        // Write all modified pages to the file.
        void commit( const PageLink link );
        // Retrieve all pages pending recovery from the file.
        PageLink recover( bool freeModifiedPages = true );
        // Clean-up page pool.
        PageHeader* clean();
        // Determine the capacity of pages stored in the file path.
        static PageSize pageCapacity( const std::string path );
    };

} // namespace BTree

#endif // PERSISTENT_PAGE_POOL_H
