#include "PersistentPagePool.h"
#include <iostream>
#include <fstream>

// ToDo: Provide transaction behaviour for file updates.
// ToDo: Lazy read of pages from file.
// ToDo: Swap pages to/from file to save memory based on usage count.

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
                PageLink link( index );
                PageHeader& page = const_cast<PageHeader&>( access( link ) );
                file.read( reinterpret_cast<char*>( &page ), pageSize );
                if (!file.good()) throw string( signature ) + " - File read error";
                if (page.capacity != pageSize) throw string( signature ) + "  - Invalid page size";
                if (page.free == 1) {
                    if ((page.modified != 0) || (page.persistent != 0) || (page.recover != 0)) {
                        throw string( signature ) + " - Read corrupt free page";
                    }
                    freePages.push_back( link );
                } else {
                    if ((page.modified != 0) || (page.persistent != 1) || (page.recover != 0)) {
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
    void PersistentPagePool::commit( const PageLink link ) {
        static const char* signature( "void PersistentPagePool::commit( const PageLink link )" );
        // Ensure committed file contains only pages that either belong to the committed B-Tree
        // (marked as persistent) or are free (marked as free). Note that we may need to write
        // free pages to file as pages of the committed B-Tree need not all be contiguous in the
        // persistent page file.
        // Purge modified list of free pages, modified pages may subsequently have been freed.
        PageLink largest;
        vector<PageLink> purgedPages;
        for ( auto link : modifiedPages ) {
            const PageHeader& modifiedPage = access( link );
            if (modifiedPage.free == 0) {
                purgedPages.push_back( link );
                if (largest.null() || (largest < link)) largest = link;
            }
        }
        modifiedPages = purgedPages;
        // Now look for freed persistent pages. These must be freed-up in the
        // persistent store (as they will be orphaned when recovered).
        // Simultaneously, look for non-persistent free pages with an PageLink less than the largest
        // PageLink being commited. These will create holes in the persistent store file.
        vector<PageLink> freedPages;
        for ( auto link : freePages ) {
            const PageHeader& freePage = access( link );
            if ((freePage.persistent == 1) || largest.null() || (link < largest)) freedPages.push_back( link );
        }
        // ToDo: Truncate file to largest persistent page.
        fstream file;
        // First try opening existing file for update (without actually reading).
        // In this case, the file exists and already contains (persistent) data.
        file.open( fileName, ios::in | ios::out | ios::binary );
        // If that fails (i.e., file does not exist) open file for output only.
        // This is the first time the persistent file is being written.
        if (!file.good()) file.open( fileName, ios::out | ios::binary );
        if (file.good()) {
            // Write all modified pages to file as persistent pages.
            for ( auto link : modifiedPages ) {
                const PageHeader& page = access( link );
                page.free = 0;
                page.modified = 0;
                page.persistent = 1;
                page.recover = 0;
                file.seekp( ((link.index * page.capacity) + sizeof(PageHeader)), file.beg );
                file.write( reinterpret_cast<const char*>( &page ), page.capacity );
                if (!file.good()) throw string( signature ) + " - File write error";
            }
            // Write all freed persistent pages as free pages to file.
            for ( auto link : freedPages ) {
                const PageHeader& page = access( link );
                page.free = 1;
                page.modified = 0;
                page.persistent = 0;
                page.recover = 0;
                file.seekp( ((link.index * page.capacity) + sizeof(PageHeader)), file.beg );
                file.write( reinterpret_cast<const char*>( &page ), page.capacity );
                if (!file.good()) throw string( signature ) + " - File write error";
            }
            // Finally, write root page header to file.
            file.seekp( 0, file.beg );
            const PageHeader& rootPage = access( link );
            file.write( reinterpret_cast<const char*>( &rootPage ), sizeof(PageHeader) );
            if (!file.good()) throw string( signature ) + " - File write error";
            file.close();
        } else {
            throw string( signature ) + " - Could not open file";
        }
        // Discard all outstanding recover requests (possibly in free or modified list).
        for (auto link : recoverPages) access( link ).recover = 0;
        recoverPages.clear();
        PagePool::commit( link );
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
    PageLink PersistentPagePool::recover( bool freeModifiedPages ) {
        static const char* signature( "PageLink PersistentPagePool::recover( bool freeModifiedPages )" );
        // Purge free list of recover pages.
        // Pages to be recovered may reside in free (or modified) list when reusing copy-on-update page memory.
        vector<PageLink> purgedPages;
        for ( auto link : freePages ) {
            const PageHeader& freePage = access( link );
            if (freePage.recover == 1) {
                freePage.free = 0;
            } else {
                purgedPages.push_back( link );
            }
        }
        freePages = purgedPages;
        purgedPages.clear();
        // Purge modified list of pages to be recovered
        for ( auto link : modifiedPages ) {
            const PageHeader& modifiedPage = access( link );
            if (modifiedPage.recover == 1) {
                modifiedPage.recover = 0;
            } else {
                purgedPages.push_back( link );
            }
        }
        modifiedPages = purgedPages;
        fstream file( fileName, ios::in | ios::binary );
        if (file.good()) {
            PageHeader root;
            file.seekg( 0, file.beg );
            file.read( reinterpret_cast<char*>( &root ), sizeof(PageHeader) );
            if (!file.good()) throw string( signature ) + " - File read error";
            if (root.capacity != capacity) throw string( signature ) + " - Invalid root page size";
            else if (root.page != commitLink) throw string( signature ) + " - Mismatched root link";
            // Read all pages marked for recover from file.
            for ( auto link : recoverPages ) {
                PageHeader& page = const_cast<PageHeader&>( access( link ) );
                file.seekg( ((link.index * page.capacity) + sizeof(PageHeader)), file.beg );
                file.read( reinterpret_cast<char*>( const_cast<PageHeader*>( &page ) ), capacity );
                if (!file.good()) throw string( signature ) + " - File read error";
                if (page.capacity != capacity) throw string( signature ) + " - Invalid page size";
                // Sanity check, ensure page has expected attributes.
                // These should match attributes defined by commit()
                if ((page.free != 0) || (page.modified != 0) || (page.persistent != 1) || (page.recover != 0)) {
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

