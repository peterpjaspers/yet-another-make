#include "PersistentPagePool.h"
#include <iostream>
#include <fstream>

// ToDo: Provide transaction behaviour for file updates.
// ToDo: Lazy read of pages from file.
// ToDo: Swap pages to/from file to save memory based on usage count.

using namespace std;

namespace BTree {

    // Page pool with persistent store to file.

    // Create a PersistentPagePool on a file.
    // If the file exists, the page pool is updated to reflect file content.
    // If the file does not exist, an empty page pool is created. 
    PersistentPagePool::PersistentPagePool( PageSize pageSize, const string path ) : PagePool( pageSize ) {
        static const std::string signature( "PersistentPagePool( PageSize pageSize, const string path )" );
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
                if (!file.good()) throw signature + " - File read error";
                if (root.capacity != pageSize) throw signature + " - Invalid root page size";
                linkRoot = root.page;
                for (size_t index = 0; index < pageCount; index++) {
                    void* buffer = malloc( pageSize );
                    file.read( reinterpret_cast<char*>( buffer ), pageSize );
                    if (!file.good()) throw signature + " - File read error";
                    PageHeader* page = reinterpret_cast<PageHeader*>( buffer );
                    if (page->capacity != pageSize) throw signature + "  - Invalid page size";
                    else if (page->page == linkRoot) {
                        // Validate that root page content matches assigned root
                        if (
                            (page->depth != root.depth) ||
                            (page->count != root.count) ||
                            (page->split != root.split)
                        ) {
                            throw signature + " - Mismatched root page content";
                        }
                    }
                    pages[ index ] = page;
                }
            } else {
                throw signature + " - Page file must contain at least one page";
            }
            file.close();
        }
    };

    // Write all page modifications (since the last commit or its creation) to persistent store.
    // The given PageLink defines the (new) root of the updated B-tree.
    void PersistentPagePool::commit( const PageLink link ) {
        static const std::string signature( "void PersistentPagePool::commit( const PageLink link )" );
        // Discard all outstanding recover requests.
        for (int i = 0; i < recoverPages.size(); i++) pages[ recoverPages[ i ].index ]->recover = 0;
        recoverPages.clear();
        // Purge modified list of free pages, modified pages may have been freed.
        vector<PageLink> modifiedFreePages;
        for (int i = 0; i < modifiedPages.size(); i++) {
            const PageHeader* modifiedPage = pages[ modifiedPages[ i ].index ];
            if (modifiedPage->free == 0) modifiedFreePages.push_back( modifiedPage->page );
        }
        modifiedPages = modifiedFreePages;
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
                file.seekp( ((page->page.index * page->capacity) + sizeof(PageHeader)), file.beg );
                file.write( reinterpret_cast<const char*>( page ), page->capacity );
                if (!file.good()) throw signature + " - File write error";
            }
            // Write root page header to file.
            file.seekp( 0, file.beg );
            PageHeader* root = reference( link );
            file.write( reinterpret_cast<char*>( root ), sizeof(PageHeader) );
            if (!file.good()) throw signature + " - File write error";
            file.close();
        } else {
            throw signature + " - Could not open file";
        }
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
        static const std::string signature( "PageLink PersistentPagePool::recover( bool freeModifiedPages )" );
        // Purge modified list of recover pages.
        vector<PageLink> modifiedRecoverPages;
        for (int i = 0; i < modifiedPages.size(); i++) {
            const PageHeader* modifiedPage = pages[ modifiedPages[ i ].index ];
            if (modifiedPage->recover == 0) modifiedRecoverPages.push_back( modifiedPage->page );
        }
        modifiedPages = modifiedRecoverPages;
        // Purge modified list of free pages.
        // Modified pages may subsequently be freed.
        vector<PageLink> modifiedFreePages;
        for (int i = 0; i < modifiedPages.size(); i++) {
            const PageHeader* modifiedPage = pages[ modifiedPages[ i ].index ];
            if (modifiedPage->free == 0) modifiedFreePages.push_back( modifiedPage->page );
        }
        modifiedPages = modifiedFreePages;
        // Purge free list of recover pages.
        // Pages to be recovered may reside in free list when reusing copy-on-update page memory.
        vector<PageLink> freeRecoverPages;
        for (int i = 0; i < freePages.size(); i++) {
            const PageHeader* freePage = pages[ freePages[ i ].index ];
            if (freePage->recover == 0) {
                freeRecoverPages.push_back( freePage->page );
            } else {
                freePage->free = 0;
            }
        }
        freePages = freeRecoverPages;
        fstream file( fileName, ios::in | ios::binary );
        if (file.good()) {
            PageHeader root;
            file.seekg( 0, file.beg );
            file.read( reinterpret_cast<char*>( &root ), sizeof(PageHeader) );
            if (!file.good()) throw signature + " - File read error";
            if (root.capacity != capacity) throw signature + " - Invalid root page size";
            else if (root.page != linkRoot) throw signature + " - Mismatched root link";
            // Read all pages marked for recover from file.
            for (size_t p = 0; p < recoverPages.size(); p++) {
                PageHeader* page = pages[ recoverPages[ p ].index ];
                file.seekg( ((page->page.index * page->capacity) + sizeof(PageHeader)), file.beg );
                file.read( reinterpret_cast<char*>( page ), capacity );
                if (!file.good()) throw signature + " - File read error";
                if (page->capacity != capacity) throw signature + " - Invalid page size";
                else if (page->page == linkRoot) {
                    // Validate that root page content matches assigned root
                    if (
                        (page->depth != root.depth) ||
                        (page->count != root.count) ||
                        (page->split != root.split)
                    ) {
                        throw signature + " - Mismatched root page content";
                    }
                }
            }
            file.close();
        }
        recoverPages.clear();
        return PagePool::recover( true );
    };

    // Determine page capacity of stored page pool.
    // Returns zero if file does not exist or could not be accessed.
    PageSize PersistentPagePool::pageCapacity( const std::string path ) {
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

