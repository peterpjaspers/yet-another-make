#include "PersistentPagePool.h"
#include <iostream>
#include <fstream>

// ToDo: Provide transaction behaviour for file updates.
// ToDo: Lazy read of pages from file.
// ToDo: Swap pages to/from file to save memory based on usage count.
// ToDo: Truncate file to largest persistent page during commit (to reduce size of persistent store).

using namespace std;

namespace BTree {

    // Create a PersistentPagePool on a file.
    // If the file exists, the page pool is updated to reflect file content.
    // If the file does not exist, an empty page pool is created. 
    PersistentPagePool::PersistentPagePool( PageSize pageSize, const string path ) : PagePool( pageSize ) {
        static const char* signature( "PersistentPagePool( PageSize pageSize, const string path )" );
        fileName = path;
        fstream file( fileName, ios::in | ios::binary );
        if (file.good()) {
            file.seekg( 0, file.end );
            size_t fileSize = file.tellg();
            size_t pageCount = ((fileSize - sizeof(PageHeader)) / pageSize);
            if (pageCount == 0) throw string( signature ) + " - Page file must contain at least one page";
            pages.assign( pageCount, nullptr );
            PageHeader root;
            file.seekg( 0, file.beg );
            file.read( reinterpret_cast<char*>( &root ), sizeof(PageHeader) );
            if (!file.good()) throw string( signature ) + " - File read error";
            if (root.capacity != pageSize) throw string( signature ) + " - File page size does not match requested page size";
            if (fileSize != ((pageCount * pageSize) + sizeof(PageHeader))) throw string( signature ) + " - Invalid page file size";
            commitLink = root.page;
            for (size_t index = 0; index < pageCount; ++index) {
                pages[ index ] = reinterpret_cast<PageHeader*>( malloc( pageSize ) );
                PageLink link( static_cast<unsigned int>(index) );
                PageHeader& page = const_cast<PageHeader&>( access( link ) );
                file.read( reinterpret_cast<char*>( &page ), pageSize );
                if (!file.good()) throw string( signature ) + " - File read error";
                // Now ensure that read file page content is valid
                if (page.free == 1) {
                    if (
                        (page.modified != 0) ||
                        (page.persistent != 0) ||
                        (page.recover != 0) ||
                        (page.stored != 1) ||
                        (page.capacity != capacity)
                    ) {
                        throw string( signature ) + " - Read corrupt free page";
                    }
                    freePages.push_back( link );
                } else {
                    if (
                        (page.modified != 0) ||
                        (page.persistent != 1) ||
                        (page.recover != 0) ||
                        (page.stored != 1) ||
                        (page.capacity != capacity)
                    ) {
                        throw string( signature ) + " - Read corrupt persistent page";
                    }
                }
                if (link == commitLink) {
                    // Validate that root page content matches root (header)
                    if ((page.depth != root.depth) || (page.count != root.count) || (page.split != root.split)) {
                        throw string( signature ) + " - Mismatched root page content";
                    }
                }
            }
            file.close();
        }
    };

    // Write all page modifications (since the last commit or its creation) to persistent store.
    // The given PageLink defines the (new) root of the updated B-tree.
    void PersistentPagePool::commit( const PageLink link, BTreeStatistics* stats ) {
        static const char* signature( "void PersistentPagePool::commit( const PageLink link, BTreeStatistics* stats )" );
        // Ensure committed file contains only pages that either belong to the committed B-Tree
        // (marked as persistent) or are free (marked as free). We may need to write free pages
        // as pages of the committed B-Tree need not all be contiguous in the file. No holes
        // are allowed as all pages of the persistent file must be readable and valid when
        // reconstructing the (entire) page pool from persistent store.
        // First, purge modified list of free pages (modified pages may subsequently have been freed).
        PageLink largest; // Keep track of largest link index to enable truncating persistent file
        vector<PageLink> keepPages;
        for ( auto link : modifiedPages ) {
            const PageHeader& modifiedPage = access( link );
            if (modifiedPage.free == 0) {
                keepPages.push_back( link );
                if (largest.null() || (largest < link)) largest = link;
            }
        }
        modifiedPages = keepPages;
        fstream file;
        // First try opening existing file for update (without actually reading).
        // In this case, the file exists and already contains (persistent) data.
        file.open( fileName, ios::in | ios::out | ios::binary );
        if (file.good()) {
            // Determine current number of pages in file...
            file.seekg( 0, file.end );
            size_t fileSize = file.tellg();
            size_t pageCount = ((fileSize - sizeof(PageHeader)) / capacity);
        } else {
            // File does not exist, open file for output only.
            // This is the first time the persistent file is being written.
            file.open( fileName, ios::out | ios::binary );
            if (!file.good()) throw string( signature ) + " - Could not open file";
        }
        // Write all modified pages to file as persistent pages.
        for ( auto link : modifiedPages ) {
            if (stats) stats->pageWrites += 1;
            const PageHeader& page = access( link );
            page.free = 0;
            page.modified = 0;
            page.persistent = 1;
            page.recover = 0;
            page.stored = 1;
            file.seekp( ((link.index * page.capacity) + sizeof(PageHeader)), file.beg );
            file.write( reinterpret_cast<const char*>( &page ), page.capacity );
            if (!file.good()) throw string( signature ) + " - Persistent page file write error";
        }
        // Now look for pages to be freed in store.
        vector<PageLink> freedPages;
        for (auto link : freePages) {
            const PageHeader& freePage = access( link );
            if ((freePage.stored == 0) || (freePage.persistent == 1)) freedPages.push_back( link );
        }
        // Write all un-stored or persistent free pages as free pages to file.
        for ( auto link : freedPages ) {
            if (stats) stats->pageWrites += 1;
            const PageHeader& page = access( link );
            page.free = 1;
            page.modified = 0;
            page.persistent = 0;
            page.recover = 0;
            page.stored = 1;
            file.seekp( ((link.index * page.capacity) + sizeof(PageHeader)), file.beg );
            file.write( reinterpret_cast<const char*>( &page ), page.capacity );
            if (!file.good()) throw string( signature ) + " - Free page file write error";
        }
        // Finally, write root page header to file.
        file.seekp( 0, file.beg );
        const PageHeader& rootPage = access( link );
        file.write( reinterpret_cast<const char*>( &rootPage ), sizeof(PageHeader) );
        if (!file.good()) throw string( signature ) + " - Root header file write error";
        file.close();
        // Discard all outstanding recover requests (possibly in free or modified list).
        for (auto link : recoverPages) access( link ).recover = 0;
        recoverPages.clear();
        PagePool::commit( link, stats );
    };

    // Mark page as pending recover and queue page for (possible) recover.
    // Page will be read from file with next recover() call, unless preceeded by a call to commit().
    // Only persistent pages (i.e., pages that have previously been commited) are actually recovered.
    // Page memory is optionally reused (frees page when resue == true).
    void PersistentPagePool::recover( const PageHeader& page, bool reuse ) {
        if ((page.persistent == 1) && (page.recover == 0)) {
            recoverPages.push_back( page.page );
            page.recover = 1;
        }
        if (reuse) free( page.page );
    };

    // Discard all page modifications by reverting to persistent store content.
    // This effectively recovers the B-tree to the state of the last commit.
    // Returns PageLink of the recoverd B-tree root page.
    PageLink PersistentPagePool::recover( bool freeModifiedPages, BTreeStatistics* stats ) {
        static const char* signature( "PageLink PersistentPagePool::recover( bool freeModifiedPages )" );
        // Pages to be recovered may reside in free (and/or modified) list when reusing copy-on-update pages.
        // Remove recover pages from free list.
        vector<PageLink> keepPages;
        for ( auto link : freePages ) {
            const PageHeader& freePage = access( link );
            freePage.modified = 0;
            if (freePage.recover == 1) {
                freePage.free = 0;
            } else {
                if (freePage.persistent == 1) throw string( signature ) + " - Persistent page free without being recovered";
                keepPages.push_back( link );
            }
        }
        freePages = keepPages;
        // Remove recover pages from modified list.
        keepPages.clear();
        for ( auto link : modifiedPages ) {
            auto modifiedPage = access( link );
            if (modifiedPage.recover == 1) {
                modifiedPage.recover = 0;
            } else {
                if (modifiedPage.persistent == 1) throw string( signature ) + " - Persistent page modified without being recovered";
                keepPages.push_back( link );
            }
        }
        modifiedPages = keepPages;
        // Now read all pages to be recovered from file.
        fstream file( fileName, ios::in | ios::binary );
        if (file.good()) {
            PageHeader root;
            file.seekg( 0, file.beg );
            file.read( reinterpret_cast<char*>( &root ), sizeof(PageHeader) );
            if (!file.good()) throw string( signature ) + " - Error reading root";
            if (root.capacity != capacity) throw string( signature ) + " - Invalid root page size";
            if (root.page != commitLink) throw string( signature ) + " - Mismatched root link";
            // Read all pages marked for recover from file.
            for ( auto link : recoverPages ) {
                if (stats) stats->pageReads += 1;
                PageHeader& page = const_cast<PageHeader&>( access( link ) );
                if (page.persistent == 0) throw string( signature ) + " - Non-persistent page being recovered";
                file.seekg( ((link.index * page.capacity) + sizeof(PageHeader)), file.beg );
                file.read( reinterpret_cast<char*>( &page ), capacity );
                if (!file.good()) throw string( signature ) + " - File read error";
                // Sanity check, ensure page has expected attributes and capacity.
                // These should match attributes defined by commit()
                if (
                    (page.free != 0) ||
                    (page.modified != 0) ||
                    (page.persistent != 1) ||
                    (page.recover != 0) ||
                    (page.stored != 1) ||
                    (page.capacity != capacity)
                ) {
                    throw string( signature ) + " - Recovering corrupt page";
                }
                if (link == commitLink) {
                    // Validate that root page content matches assigned root
                    if ((page.depth != root.depth) || (page.count != root.count) || (page.split != root.split)) {
                        throw string( signature ) + " - Mismatched root page content";
                    }
                }
            }
            file.close();
        }
        recoverPages.clear();
        return PagePool::recover( freeModifiedPages );
    };

    // Update page pool administration to pristine state consisting soley of persistent pages.
    // All other pages are inserted in the free pages list. When complete, no pages are
    // marked as modified or recover.
    PageHeader* PersistentPagePool::clean() {
        freePages.clear();
        modifiedPages.clear();
        recoverPages.clear();
        for ( auto page : pages ) {
            page->free = 0;
            page->modified = 0;
            page->recover = 0;
            if (page->persistent == 0) {
                page->free = 1;
                freePages.push_back( page->page );
            }
        }
        return PagePool::clean();
    }

    // Determine page capacity of stored page pool.
    // Returns zero if file does not exist or could not be accessed.
    PageSize PersistentPagePool::pageCapacity( const string path ) {
        PageSize capacity = 0;
        fstream file;
        file.open( path, ios::in | ios::binary );
        if (file.good()) {
            PageHeader root;
            file.seekg( 0, file.beg );
            file.read( reinterpret_cast<char*>( &root ), sizeof(PageHeader) );
            if (file.good()) capacity = root.capacity;
        }
        file.close();
        return( capacity );
    };


} // namespace BTree

