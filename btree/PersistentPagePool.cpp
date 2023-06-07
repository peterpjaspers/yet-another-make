#include "PersistentPagePool.h"
#include <iostream>
#include <fstream>

// ToDo: Provide transaction behaviour for file updates.
// ToDo: Lazy read of pages from file.
// ToDo: Swap pages to/from file to save memory based on usage count.

using namespace std;

namespace BTree {

    // Page pool with persistent storage in file.

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
            if (0 < pageCount) {
                pages.assign( pageCount, nullptr );
                PageHeader root;
                file.seekg( 0, file.beg );
                file.read( reinterpret_cast<char*>( &root ), sizeof(PageHeader) );
                if (!file.good()) throw string( signature ) + " - File read error";
                if (root.capacity != pageSize) throw string( signature ) + " - Invalid root page size";
                commitLink = root.page;
                for (size_t index = 0; index < pageCount; index++) {
                    void* buffer = malloc( pageSize );
                    file.read( reinterpret_cast<char*>( buffer ), pageSize );
                    if (!file.good()) throw string( signature ) + " - File read error";
                    PageHeader* page = reinterpret_cast<PageHeader*>( buffer );
                    if (page->capacity != pageSize) throw string( signature ) + "  - Invalid page size";
                    else if (page->page == commitLink) {
                        // Validate that root page content matches assigned root
                        if (
                            (page->depth != root.depth) ||
                            (page->count != root.count) ||
                            (page->split != root.split)
                        ) {
                            throw string( signature ) + " - Mismatched root page content";
                        }
                    }
                    pages[ index ] = page;
                    if (page->free == 1) freePages.push_back( page->page );
                }
            } else {
                throw string( signature ) + " - Page file must contain at least one page";
            }
            file.close();
        }
    };

    PersistentPagePool::~PersistentPagePool() {
        // Ensure entire file has been written with valid data by filling holes in file.
        // Not all consecutive pages may have been written after multiple modifications
        // between commits. A hole is a non-persistent page with an index less than
        // a persistent page.
        // First look for persistent page with largest index.
        int count = pages.size();
        PageLink largest;
        for (int i = 0; i < count; i++) {
            PageHeader* page = pages[ count - i - 1 ];
            if (page->persistent == 1) {
                largest = page->page;
                break;
            }
        }
        if ( !largest.null() ) {
            // There are persistent pages, collect holes (if any)
            vector<PageLink> holes;
            for (int i = 0; i < largest.index; i++) {
                PageHeader* page = pages[ i ];
                if (page->persistent == 0) holes.push_back( page->page );
            }
            if (0 < holes.size()) {
                // There are holes, write those pages to file as free pages.
                fstream file( fileName, ios::in | ios::out | ios::binary );
                if (file.good()) {
                    // Perform consistency check:
                    // File size must match largest persistent page index
                    file.seekg( 0, file.end );
                    size_t fileSize = file.tellg();
                    size_t pageCount = ((fileSize - sizeof(PageHeader)) / capacity);
                    if (pageCount == (largest.index + 1)) {
                        for (auto link : holes) {
                            PageHeader* page =  pages[ link.index ];
                            page->free = 1; // Mark hole as free page...
                            file.seekp( ((page->page.index * page->capacity) + sizeof(PageHeader)), file.beg );
                            file.write( reinterpret_cast<const char*>( page ), page->capacity );
                            if (!file.good()) break;
                        }
                    }
                }
                file.close();
            }
        }
    };

    // Write all page modifications (since the last commit or its creation) to persistent store.
    // The given PageLink defines the (new) root of the updated B-tree.
    void PersistentPagePool::commit( const PageLink link ) {
        static const char* signature( "void PersistentPagePool::commit( const PageLink link )" );
        // Purge modified list of free pages, modified pages may have been freed.
        vector<PageLink> nonFreeModifiedPages;
        for (int i = 0; i < modifiedPages.size(); i++) {
            const PageHeader* modifiedPage = pages[ modifiedPages[ i ].index ];
            if (modifiedPage->free == 0) nonFreeModifiedPages.push_back( modifiedPage->page );
        }
        modifiedPages = nonFreeModifiedPages;
        fstream file;
        // First try opening existing file for update (without actually reading).
        file.open( fileName, ios::in | ios::out | ios::binary );
        // If that fails (i.e., file does not exist) open file for output.
        if (!file.good()) file.open( fileName, ios::out | ios::binary );
        if (file.good()) {
            // Write all modified pages to file.
            for (size_t p = 0; p < modifiedPages.size(); p++) {
                const PageHeader* page = pages[ modifiedPages[ p ].index ];
                page->modified = 0;
                page->persistent = 1;
                page->recover = 0;
                file.seekp( ((page->page.index * page->capacity) + sizeof(PageHeader)), file.beg );
                file.write( reinterpret_cast<const char*>( page ), page->capacity );
                if (!file.good()) throw string( signature ) + " - File write error";
            }
            // Finally, write root page header to file.
            file.seekp( 0, file.beg );
            PageHeader* rootPage = reference( link );
            file.write( reinterpret_cast<char*>( rootPage ), sizeof(PageHeader) );
            if (!file.good()) throw string( signature ) + " - File write error";
            file.close();
        } else {
            throw string( signature ) + " - Could not open file";
        }
        // Discard all outstanding recover requests.
        for (int i = 0; i < recoverPages.size(); i++) pages[ recoverPages[ i ].index ]->recover = 0;
        recoverPages.clear();
        PagePool::commit( link );
    };

    // Mark page as pending recover and queue page for (possible) recover.
    // Page will be read from file with next recover() call, unless preceeded by a call to commit().
    // Only persistent pages are actually recovered (does nothing if page is not persistent).
    // Page memory is optionally reused (frees page when resue == true).
    void PersistentPagePool::recover( const PageHeader& page, bool reuse ) {
        if (page.persistent == 0) return;
        if (page.recover == 0) {
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
        // Purge modified list of recover pages.
        vector<PageLink> nonRecoverModifiedPages;
        for (int i = 0; i < modifiedPages.size(); i++) {
            const PageHeader* modifiedPage = pages[ modifiedPages[ i ].index ];
            if (modifiedPage->recover == 0) nonRecoverModifiedPages.push_back( modifiedPage->page );
        }
        modifiedPages = nonRecoverModifiedPages;
        // Purge modified list of free pages.
        // Modified pages may subsequently be freed.
        vector<PageLink> nonFreeModifiedPages;
        for (int i = 0; i < modifiedPages.size(); i++) {
            const PageHeader* modifiedPage = pages[ modifiedPages[ i ].index ];
            if (modifiedPage->free == 0) nonFreeModifiedPages.push_back( modifiedPage->page );
        }
        modifiedPages = nonFreeModifiedPages;
        // Purge free list of recover pages.
        // Pages to be recovered may reside in free list when reusing copy-on-update page memory.
        vector<PageLink> nonRecoverFreePages;
        for (int i = 0; i < freePages.size(); i++) {
            const PageHeader* freePage = pages[ freePages[ i ].index ];
            if (freePage->recover == 0) {
                nonRecoverFreePages.push_back( freePage->page );
            } else {
                freePage->free = 0;
            }
        }
        freePages = nonRecoverFreePages;
        fstream file( fileName, ios::in | ios::binary );
        if (file.good()) {
            PageHeader root;
            file.seekg( 0, file.beg );
            file.read( reinterpret_cast<char*>( &root ), sizeof(PageHeader) );
            if (!file.good()) throw string( signature ) + " - File read error";
            if (root.capacity != capacity) throw string( signature ) + " - Invalid root page size";
            else if (root.page != commitLink) throw string( signature ) + " - Mismatched root link";
            // Read all pages marked for recover from file.
            for (size_t p = 0; p < recoverPages.size(); p++) {
                PageHeader* page = pages[ recoverPages[ p ].index ];
                file.seekg( ((page->page.index * page->capacity) + sizeof(PageHeader)), file.beg );
                file.read( reinterpret_cast<char*>( page ), capacity );
                if (!file.good()) throw string( signature ) + " - File read error";
                if (page->capacity != capacity) throw string( signature ) + " - Invalid page size";
                if (page->free != 0) throw string( signature ) + " - Recovering free page";
                if (page->page == commitLink) {
                    // Validate that root page content matches assigned root
                    if (
                        (page->depth != root.depth) ||
                        (page->count != root.count) ||
                        (page->split != root.split)
                    ) {
                        throw string( signature ) + " - Mismatched root page content";
                    }
                }
            }
            file.close();
        }
        recoverPages.clear();
        return PagePool::recover( freeModifiedPages );
    };

    // Update page pool administration to pristine state consiting soley of persistent pages.
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

