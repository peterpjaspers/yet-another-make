#include "BTree.h"
#include "PersistentPagePool.h"
#include <map>
#include <set>
#include <vector>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <random>
#include <algorithm>
#include <cassert>
#include <utility>
//#include <boost/filesystem.hpp>

using namespace BTree;
using namespace std;
using namespace std::filesystem;

#define each(i,count) for(int i = 0; i < int(count); ++i)
#define times(count) for(int i = 0; i < int(count); ++i)

// ToDo: Validate = and [] operators on B-Tree

// Test program to measure B-Tree validity.
// B-Tree validity is performed via a comprehensive set of use cases.
// Tests are performed on four types of B-Trees:
//
//   Uint32 -> Uint32           32-bit scalar keys -> 32-bit scalar values
//   [ Uint16 ] -> Uint32       16-bit array keys  -> 32-bit scalar values
//   Uint32 -> [ Uint16 ]       32-bit scalar keys -> 16-bit array values
//   [ Uint16 ] -> [ Uint16 ]   16-bit array keys  -> 16-bit array values
//
// An administration of which entries are in the B-Tree is maintained in an std::map data structure.
// This enables validating which entries should be present in the B-Tree and which should not.
// And additional std::map is used for validation of transaction behavior. This map contains
// values held in the B-Tree at the last commit.

// B-Tree page size is kept low to maximize B-Tree depth improving test coverage
// MaxArray chosen to comform to maximum entry size constraint
const int BTreePageSize = 256;
const int MinArray = 2;
const int MaxArray = 14;
// Enable generating exceptions when accessing non-existing keys.
// Set to true for full coverage (presumably when compiled with optimization)
// Set to false when debugging to avoid generating intentional exceptions.
#ifdef NDEBUG
    const bool TryUnexpectedKeys = true;
#else
    const bool TryUnexpectedKeys = false;
#endif

// Number of attempts to detect unexpected B-Tree content
const int ProbeCount = 100;
const int EnduranceCount = 10;
const int TransactionCount = 10;

mt19937 gen32;

inline uint16_t generateUint16() {
    return( 1 + (gen32() % 10000) );
}
inline uint32_t generateUint32() {
    return( 1 + (gen32() % 1000000000) );
}
void generateUint16Array( vector<uint16_t>& value ) {
    value.clear();
    times( (MinArray  + (gen32() % (MaxArray - MinArray))) ) value.push_back( generateUint16() );
}

void logUint16Array( ostream& log, const uint16_t* value, PageSize valueSize ) {
    log << "[ ";
    if (0 < valueSize) {
        each( i, (valueSize - 1) ) log << value[ i ] << ", ";
        log << value[ valueSize - 1 ];
    }
    log << " ]";
}

void logPageList( ostream& log, const vector<PageLink>& list, const string tag ) {
    log << tag << " pages [";
    if (0 < list.size()) {
        log << " " << list[ 0 ];
        for ( int i = 1; i < list.size(); ++i ) log << ", " << list[ i ];
    }
    log << " ]\n";
}

struct Uint16ArrayCompare {
    bool operator()( const vector<uint16_t>* lhs, const vector<uint16_t>* rhs ) const {
        if (compare( lhs->data(), PageSize(lhs->size()), rhs->data(), PageSize(rhs->size()) ) < 0) return( true );
        return( false );
    }
    static int compare( const uint16_t* lhs, PageSize ln, const uint16_t* rhs, PageSize rn ) {
        each( i, ((ln < rn) ? ln : rn) ) {
            if (lhs[ i ] < rhs[ i ]) return( -1 ); else if (rhs[ i ] < lhs[ i ]) return( +1 );
        }
        if (rn < ln) return( -1 );
        if (ln < rn) return( +1 );
        return( 0 );
    }
};

size_t generateIndex( size_t range ) {
    return( gen32() % range );
}

template< class K, class V >
class TreeTester {
protected:
    const string directory;
    const string fileName;
    bool isPersistent;
    ostream& log;
    PagePool* pool;
    Tree< K, V >* tree;
public:
    enum KeyOrder {
        Forward,
        Reverse,
        Random
    };
    string order2String ( KeyOrder order ) {
        if (order == Forward) return "Forward";
        else if (order == Reverse) return "Reverse";
        else if (order == Random) return "Random";
        return "Unknown";
    }

    TreeTester() = delete;
    TreeTester( const string dir, const string file, ostream& logStream ) :
        directory( dir ),
        fileName( file ),
        log( logStream ),
        isPersistent( false ),
        pool( nullptr ),
        tree( nullptr )
    {}
    ~TreeTester() {
        if (tree != nullptr) destroyTree();
        if (pool != nullptr) destroyPool();
        if (exists( persistentPath() )) deletePersistentStore();
    }
    void persistent( bool p ) {
        if (pool != nullptr) log << "Cannot change persistency while pool exists!\n";
        isPersistent = p;
    }
    string persistentPath() const { return directory + "/" + fileName + ".bt"; }
    void createPool() {
        if (pool == nullptr) {
            if ( isPersistent ) {
                log << "Constructing persistent page pool on " << persistentPath() << " ...\n" << flush;
                string poolFile = persistentPath();
                PageSize storedPageSize = PersistentPagePool::pageCapacity( poolFile );
                pool = new PersistentPagePool( ((0 < storedPageSize) ? storedPageSize : BTreePageSize), poolFile );
            } else {
                pool = new PagePool( BTreePageSize );                
            }
        } else {
            log << "Page pool already exists!\n" << flush;
        }
    }
    void destroyPool() {
        log << "Deleting page pool ...\n" << flush;
        if (tree != nullptr) { log << "B-Tree still exists!\n" << flush; }
        if (pool != nullptr) {
            if ( isPersistent ) delete static_cast<PersistentPagePool*>( pool );
            else delete pool;
            pool = nullptr;
        } else {
            log << "Page pool does not exist!\n" << flush;
        }
    }
    virtual void deletePersistentStore() {
        if ( isPersistent ) {
            log << "Deleting persistent store " << persistentPath() << " ...\n" << flush;
            if (pool != nullptr) log << "Page pool still exists!\n" << flush;
            remove( persistentPath() );
        } else {
            log << "Page pool is not persistent!\n";
        }
    }
    virtual void createTree() {
        log << "Constructing B-Tree...\n" << flush;
        if (tree == nullptr) {
            tree = new Tree< K, V >( *pool );
            tree->enableStatistics();
        } else {
            log << "B-Tree already exists!\n" << flush;
        }
    }
    virtual void destroyTree() {
        log << "Deleting B-Tree...\n" << flush;
        if (tree != nullptr) {
            logStatistics();
            delete( tree );
            tree = nullptr;
        } else {
            log << "B-Tree does not exist!\n" << flush;
        }
    }
    PageDepth treeDepth() const { return(  (tree != nullptr) ? tree->depth() : 0 ); }
    size_t treeSize() const { return( (tree != nullptr) ? tree->size() : 0 ); }
    virtual size_t size() const = 0;

    uint32_t validatePersistentPagePool( PageSize pageSize ) const {
        uint32_t errors = 0;
        if ( isPersistent ) {
            log << "Reading from persistent page file " << persistentPath() << "\n";
            fstream file( persistentPath(), ios::in | ios::binary );
            if (file.good()) {
                file.seekg( 0, file.end );
                size_t fileSize = file.tellg();
                size_t pageCount = ((fileSize - sizeof(PageHeader)) / pageSize);
                if (pageCount == 0) {
                    log << "Page file contains less than 1 page!\n";
                    errors += 1;
                };
                PageHeader root;
                file.seekg( 0, file.beg );
                file.read( reinterpret_cast<char*>( &root ), sizeof(PageHeader) );
                if (file.good()) {
                    if (root.capacity != pageSize) {
                        log << "Root page capacity " << root.capacity << " does not match expected capacity " << pageSize << "!\n";
                        errors += 1;
                    }
                    if (fileSize != ((pageCount * pageSize) + sizeof(PageHeader))) {
                        log << "File size " << fileSize << " does not match expected size for " << pageCount << " pages!\n";
                        errors += 1;
                    }
                    auto buffer = new uint8_t[ pageSize ];
                    for (size_t index = 0; index < pageCount; ++index) {
                        file.read( reinterpret_cast<char*>( buffer ), pageSize );
                        if (!file.good()) {
                            log << "File read error on page " << index << " !\n";
                            errors += 1;
                            break;
                        }
                        auto page = reinterpret_cast<PageHeader*>( buffer );
                        if (page->free == 1) {
                            if (
                                (page->modified != 0) ||
                                (page->persistent != 0) ||
                                (page->recover != 0) ||
                                (page->stored != 1) ||
                                (page->capacity != BTreePageSize)
                            ) {
                                log << "Free page " << index << " is corrupt : "
                                    << " modified " << page->modified
                                    << ", persistent " << page->persistent
                                    << ", recover " << page->recover
                                    << ", stored " << page->stored
                                    << ", capacity " << page->capacity
                                    << "!\n";
                                errors += 1;
                            }
                        } else {
                            if (
                                (page->modified != 0) ||
                                (page->persistent != 1) ||
                                (page->recover != 0) ||
                                (page->stored != 1) ||
                                (page->capacity != BTreePageSize)
                            ) {
                                log << "Persistentent page " << index << " is corrupt : "
                                    << " modified " << page->modified
                                    << ", persistent " << page->persistent
                                    << ", recover " << page->recover
                                    << ", stored " << page->stored
                                    << ", capacity " << page->capacity
                                    << "!\n";
                                errors += 1;
                            }
                        }
                    }
                    delete[] buffer;
                } else {
                    log << "File read error on root header!\n";
                    errors += 1;

                }
                file.close();
            }
        }
        return errors;
    }
    template< class KT, class VT, bool KA, bool VA >
    pair<uint32_t,uint32_t> validateNode( set<PageLink>& pageLinks, const Page<KT,PageLink,KA,false>& node, PageDepth depth ) const {
        uint32_t errors = 0;
        uint32_t pageCount = 0;
        if (node.splitDefined()) {
            auto result = validatePage<KT,VT,KA,VA>( pageLinks, node.split(), (depth - 1) );
            errors += result.first;
            pageCount += result.second;
        }
        for (PageIndex i = 0; i < node.size(); ++i) {
            auto result = validatePage<KT,VT,KA,VA>( pageLinks, node.value( i ), (depth - 1) );
            errors += result.first;
            pageCount += result.second;
        }
        return{ errors, pageCount };

    }
    template< class KT, class VT, bool KA, bool VA >
    pair<uint32_t,uint32_t> validatePage( set<PageLink>& pageLinks, PageLink link, PageDepth depth ) const {
        uint32_t errors = 0;
        if (link.null()) {
            log << "Accessing null link!\n";
            errors += 1;
            return{ errors, 0 };
        }
        if ( pool->size() <= link.index ) {
            log << "Invalid PageLink index " << link.index << " exceeds pool size " << pool->size() << "!\n" ;
            errors += 1;
            return{ errors, 0 };
        }
        auto result = pageLinks.insert( link );
        if (!result.second) {
            log << "Malformed B-Tree (cycles or merged branches) at " << link << "!\n";
            errors += 1;
            return{ errors, 0 };
        }
        const PageHeader& page = pool->access( link );
        if (page.free == 1) {
            log << "Page " << link << " is free!\n";
            errors += 1;
        }
        if (depth == UINT16_MAX) depth = page.depth;
        if (page.depth != depth) {
            log << "Page " << link << " has mismatched depth " << page.depth << ", expected " << depth << "!\n";
            errors += 1;
        }
        uint32_t pageCount = 1;
        if (0 < page.depth) {
            auto result = validateNode<KT,VT,KA,VA>( pageLinks, *pool->page<KT,PageLink,KA,false>( &page ), depth );
            errors += result.first;
            pageCount += result.second;
        }
        return{ errors, pageCount };
    }
    template< class KT, class VT >
    uint32_t validatePagePool() const {
        uint32_t errors = 0;
        uint32_t pageCount = 0;
        // Set of PageLinks of pages belonging to B-Tree.
        // Used to detect malformed B-Tree, pure hierarchy expected.
        // Detects e.g. cyclic dependencies and nodes with multiple parents
        set<PageLink> pageLinks;
        if (tree != nullptr) {
            // Validate all pages belonging to the B-Tree (recursive from root)
            auto result = validatePage<B<KT>,B<VT>,A<KT>,A<VT>>( pageLinks, tree->rootLink(), UINT16_MAX );
            errors = result.first;
            pageCount = result.second;
        }
        // Determine average page filling (informational)...
        uint64_t totalUsage = 0;
        for (auto link : pageLinks) {
            const PageHeader& page = pool->access( link );
            if (page.depth == 0) {
                auto leaf = pool->page<B<KT>,B<VT>,A<KT>,A<VT>>( &page );
                totalUsage += leaf->filling();
            } else {
                auto node = pool->page<B<KT>,PageLink,A<KT>,false>( &page );
                totalUsage += node->filling();
            }
        }
        uint64_t capacity = (pageCount * BTreePageSize);
        log << "B-Tree size " << totalUsage
            << " bytes, capacity " << (pageCount * BTreePageSize)
            << " bytes, in " << pageCount
            << " pages, filling " << ((totalUsage * 100)/ capacity) << " %\n";
        // Now check to see if there are orphan pages.
        // An orphan is a non-free page that is not counted as a B-Tree page
        vector<PageLink> freePages;
        vector<PageLink> modifiedPages;
        vector<PageLink> recoverPages;
        vector<PageLink> persistentPages;
        for (uint32_t i = 0; i < pool->size(); ++i ) {
            const PageLink link( i );
            const PageHeader& page = pool->access( link );
            if (page.free != 0) freePages.push_back( link );
            if (page.modified != 0) modifiedPages.push_back( link );
            if (page.recover != 0) recoverPages.push_back( link );
            if (page.persistent != 0) persistentPages.push_back( link );
            if (!isPersistent && page.persistent != 0) {
                log << "Page " << page.page << " marked as persistent in non-persistent pool!\n";
                errors += 1;
            }
            if ((page.recover != 0) && (page.persistent == 0)) {
                log << "Recovering non-persistent page " << page.page << "!\n";
                errors += 1;
            }
        }
        if (freePages.size() != pool->sizeFreed()) {
            log << "Free pages list size " << pool->sizeFreed() << " does not match detected number of free pages " << freePages.size() << "!\n";
            errors += 1;
        }
        if (modifiedPages.size() != pool->sizeModified()) {
            log << "Modified pages list size " << pool->sizeModified() << " does not match detected number of modified pages " << modifiedPages.size() << "!\n";
            errors += 1;
        }
        if (recoverPages.size() != pool->sizeRecover()) {
            log << "Recover pages list size " << pool->sizeRecover() << " does not match detected number of recover pages " << recoverPages.size() << "!\n";
            errors += 1;
        }
        if (persistentPages.size() < recoverPages.size()) {
            log << "Number of recover pages " << recoverPages.size() << " exceeds number of persistent pages " << persistentPages.size() << "!/n";
        }
        if ((pageCount + freePages.size()) < pool->size()) {
            auto orphans = (pool->size() - (pageCount + freePages.size()));
            log << "Detected " << orphans << " orphans out of " << pool->size() << " pages, B-Tree used " << pageCount << " pages!\n";
            errors += 1;
        }
        log << "Page pool consists of " << pool->size() << " pages, "
            << freePages.size() << " free, "
            << modifiedPages.size() << " modified, "
            << persistentPages.size() << " persistent, "
            << recoverPages.size() << " recover.\n";
        // Following page list logs are for debugging. Enable when requried.
        // logPageList( log, freePages, "Free" );
        // logPageList( log, modifiedPages, "Modified" );
        // logPageList( log, persistentPages, "Persistent" );
        // logPageList( log, recoverPages, "Recover" );
        return errors;
    }

    virtual uint32_t validate() const {
        uint32_t errors = 0;
        statistics();
        log << "Validating page pool file...\n" << flush;
        errors += validatePersistentPagePool( BTreePageSize );
        log << "Validating page pool...\n" << flush;
        errors += validatePagePool<K,V>();
        if (errors == 0) {
            log << "Validating B-Tree" << flush;
            if (TryUnexpectedKeys) log << " (with tests on unexpected keys) ";
            log << "...\n" << flush;
            if (tree == nullptr) {
                log << "B-Tree does not exist!\n";
                errors += 1;
            } else {
                size_t tSize = treeSize();
                size_t cSize = size();
                if (treeSize() != size()) {
                    log << "Size mismatch : B-tree size " << tSize << ", expected " << cSize << "!\n";
                    errors += 1;
                }
            }
        }
        return errors;
    }
    virtual uint32_t insert( int count, KeyOrder order ) {
        log << "Inserting " << count << " keys in " << order2String(order) << " order...\n" << flush;
        return 0;
    };
    virtual uint32_t replace( int count ) {
        log << "Replacing " << count << " keys...\n" << flush;
        return 0;
    }
    virtual uint32_t erase( int count, KeyOrder order ) {
        log << "Removing " << count << " keys in " << order2String(order) << " order...\n" << flush;
        return 0;
    }
    virtual uint32_t commit() {
        uint32_t errors = 0;
        log << "Commit...\n" << flush;
        if (tree != nullptr) {
            tree->commit();
        } else {
            log << "B-Tree does not exist!\n" << flush;
            errors += 1;
        }
        return errors;
   };
    virtual uint32_t recover() {
        uint32_t errors = 0;
        log << "Recover...\n" << flush;
        if (tree != nullptr) {
            tree->recover();
        } else {
            log << "B-Tree does not exist!\n" << flush;
            errors += 1;
        }
        return errors;
    };
    virtual uint32_t assign() {
        log << "Assign...\n" << flush;
        return 0;
    }
    virtual void statistics() const {
        log << "Tree contains " << tree->size() << " entries at a depth of " << tree->depth() << ".\n";
    }
    void logStatistics() const {
        if (tree != nullptr) {
            BTreeStatistics stats;
            if (tree->statistics( stats )) {
                log << "B-Tree statistics\n"
                    << "    Insertions        " << stats.insertions << "\n"
                    << "    Retrievals        " << stats.retrievals << "\n"
                    << "    Replacements      " << stats.replacements << "\n"
                    << "    Removals          " << stats.removals << "\n"
                    << "    Finds             " << stats.finds << "\n"
                    << "    Grows             " << stats.grows << "\n"
                    << "    Page allocations  " << stats.pageAllocations << "\n"
                    << "    Page frees        " << stats.pageFrees << "\n"
                    << "    Merge attempts    " << stats.mergeAttempts << "\n"
                    << "    Page merges       " << stats.pageMerges << "\n"
                    << "    Page shifts       " << stats.pageShifts << "\n"
                    << "    Root updates      " << stats.rootUpdates << "\n"
                    << "    Split updates     " << stats.splitUpdates << "\n"
                    << "    Commits           " << stats.commits << "\n"
                    << "    Recovers          " << stats.recovers << "\n"
                    << "    Page writes       " << stats.pageWrites << "\n"
                    << "    Page reads        " << stats.pageReads << "\n";
            }
        }
    }
    void clearStatistics() const {
        if (tree != nullptr) tree->clearStatistics();
    }
    uint32_t logTree() const {
        uint32_t errors = 0;
        log << "Printing B-Tree content...\n" << flush;
        if (tree != nullptr) {
            this->log << *tree << flush;
        } else {
            log << "B-Tree does not exist!\n" << flush;
            errors += 1;
        }
        return errors;
    };
}; // class TreeTester

class Uint32Uint32TreeTester : public TreeTester< uint32_t, uint32_t > {
    vector<uint32_t> keys;
    map<uint32_t,uint32_t> content;
    map<uint32_t,uint32_t> commitedContent;
private:
    uint32_t generateUniqueKey() const {
        uint32_t key = generateUint32();
        while (content.count( key ) == 1) key = generateUint32();
        return key;
    };
    vector<uint32_t>* generateUniqueKeys( uint32_t count ) const {
        auto keys = new vector<uint32_t>();
        set<uint32_t> keySet;
        times( count ) {
            uint32_t key = generateUint32();
            while ((keySet.count( key ) == 1) || (content.count( key ) == 1)) key = generateUint32();
            keys->push_back( key );
            keySet.insert( key );
        }
        return keys;
    }
    uint32_t insertKey( uint32_t key ) {
        uint32_t errors = 0;
        uint32_t value = generateUint32();
        if (!tree->insert( key, value )) {
            log << "Insert with non-existing key " << key << " returned false!\n";
            errors += 1;
        } else {
            keys.push_back( key );
            content[ key ] = value;
        }
        return errors;
    }
    uint32_t removeKey( uint32_t key ) {
        uint32_t errors = 0;
        bool removed = tree->erase( key );
        if (!removed) {
            log << "Remove with existing key " << key << " returned false!\n";
            errors += 1;
        } else {
            content.erase( key );
        }
        return errors;
    }
public:
    Uint32Uint32TreeTester() = delete;
    Uint32Uint32TreeTester( const string dir, const string file, ostream& logStream ) :
        TreeTester<uint32_t,uint32_t>( dir, file, logStream )
    {}
    void createTree() {
        TreeTester<uint32_t,uint32_t>::createTree();
        content = commitedContent;
        for (auto entry = content.cbegin(); entry != content.cend(); ++entry) keys.push_back( entry->first );
    }
    void destroyTree() {
        TreeTester<uint32_t,uint32_t>::destroyTree();
        keys.clear();
        content.clear();
    }
    void deletePersistentStore() {
        TreeTester<uint32_t,uint32_t>::deletePersistentStore();
        commitedContent.clear();
    }
    size_t size() const { return content.size(); }
    uint32_t validate() const {
        BTreeStatistics stats;
        bool statsEnabled = ((tree != nullptr) && tree->disableStatistics( &stats ));
        uint32_t errors = TreeTester<uint32_t,uint32_t>::validate();
        if (tree != nullptr) {
            // Ensure that all expected content is actually present
            for (auto entry = content.cbegin(); entry != content.cend(); ++entry) {
                auto retrieved = tree->at( entry->first );
                if (retrieved != entry->second) {
                    log << "Key " << entry->first << " : Expected " << entry->second << ", retrieved " << retrieved << "!\n";
                    errors += 1;
                }
                bool found = tree->contains( entry->first );
                if (!found) {
                    log << "Contains with existing key " << entry->first << " returned false!\n";
                    errors += 1;
                }
                PageSize count = tree->count( entry->first );
                if (count != 1) {
                    log << "Count with existing key " << entry->first << " returned " << count << "!\n";
                    errors += 1;
                }
            }
            // Ensure that other content is not present
if (TryUnexpectedKeys) log << "Trying un-expected keys...\n";
            times( ProbeCount ) {
                uint32_t key = generateUniqueKey();
                if (TryUnexpectedKeys) {
                    try {
                        auto retrieved = tree->at( key );
                        log << "Retrieved " << retrieved << " with unexpected key " << key << "!\n";
                        errors += 1;
                    }
                    catch (...) {
                        // Exception expected, ignore
                    }
                }
                bool found = tree->contains( key );
                if (found) {
                    log << "Contains with non-existing key " << key << " returned true!\n";
                    errors += 1;
                }
                PageSize count = tree->count( key );
                if (count != 0) {
                    log << "Count with non-existing key " << key << " returned " << count << "!\n";
                    errors += 1;
                }
            }
        }
        if (statsEnabled) tree->enableStatistics( &stats );
        return errors;
    }
    uint32_t insert( int count, KeyOrder order ) {
        uint32_t errors = TreeTester<uint32_t,uint32_t>::insert( count, order );
        vector<uint32_t>* insertKeys = generateUniqueKeys( count );
        if (order == Forward) {
            sort( insertKeys->begin(), insertKeys->end() );
            for ( auto key : *insertKeys ) { errors += insertKey( key ); }
        } else if (order == Reverse) {
            sort( insertKeys->rbegin(), insertKeys->rend() );
            for ( auto key : *insertKeys ) { errors += insertKey( key ); }
        } else if (order == Random) {
            shuffle( insertKeys->begin(), insertKeys->end(), gen32 );
            for ( auto key : *insertKeys ) { errors += insertKey( key ); }
        }
        delete insertKeys;
        for( auto entry : content ) {
            if (tree->insert( entry.first, entry.second )) {
                log << "Insert with existing key " << entry.first << " returned true!\n";
                errors += 1;
            }
        }
        if (0 < errors) log << "Detected " << errors << " insert errors.\n";
        return errors;
    };
    uint32_t replace( int count ) {
        uint32_t errors = TreeTester<uint32_t,uint32_t>::replace( count );
        for( int i = 0; i < count; ++i) {
            size_t range = keys.size();
            if (0 < range) {
                uint32_t key = keys[ generateIndex( range ) ];
                uint32_t value = generateUint32();
                if (!tree->replace( key, value )) errors += 1;
                if (0 < errors) {
                    log << "Replace with existing key " << key << " returned false!\n";
                } else {
                    content[ key ] = value;
                }
            }
        }
        times( ProbeCount ) {
            uint32_t key = generateUniqueKey();
            if (tree->replace( key, generateUint32() )) {
                log << "Replace with non-existing key " << key << " returned true!\n";
                errors += 1;
            }
        }
        if (0 < errors) log << "Detected " << errors << " relpace errors.\n";
        return errors;
    }
    uint32_t erase( int count, KeyOrder order ) {
        uint32_t errors = TreeTester<uint32_t,uint32_t>::erase( count, order );
        size_t range = keys.size();
        if (range < count) count = static_cast<int>(range);
        if (order == Forward) {
            sort( keys.begin(), keys.end() );
            each( i, count ) errors += removeKey( keys[ i ] );
            keys.erase( keys.begin(), keys.begin() + count );
        } else if (order == Reverse) {
            sort( keys.rbegin(), keys.rend() );
            each( i, count ) errors += removeKey( keys[ i ] );
            keys.erase( keys.end() - count, keys.end() );
        } else if (order == Random) {
            shuffle( keys.begin(), keys.end(), gen32 );
            each( i, count ) errors += removeKey( keys[ i ] );
            keys.erase( keys.begin(), (keys.begin() + count) );
        }
        times( ProbeCount ) {
            uint32_t key = generateUniqueKey();
            if (tree->erase( key )) {
                log << "Remove with non-existing key " << key << " returned true!\n";
                errors += 1;
            }
        }
        if (0 < errors) log << "Detected " << errors << " erase errors.\n";
        return errors;
}
    uint32_t commit() {
        uint32_t errors = TreeTester<uint32_t,uint32_t>::commit();
        commitedContent = content;
        return errors;
    };
    uint32_t recover() {
        uint32_t errors = TreeTester<uint32_t,uint32_t>::recover();
        content = commitedContent;
        keys.clear();
        for (auto entry = content.cbegin(); entry != content.cend(); ++entry) keys.push_back( entry->first );
        return errors;
    };
    uint32_t assign() {
        uint32_t errors = TreeTester<uint32_t,uint32_t>::assign();
        PagePool* temp = new PagePool( BTreePageSize * 2 );
        {
            // Enclose in block to destruct B-Tree prior to destructing
            // dynamically allocated PagePool
            Tree<uint32_t,uint32_t> copy( *temp );
            copy.assign( *tree );
            size_t n = copy.size();
            if (n != content.size()) {
                log << "Expected size after assignment is " << content.size() << ", actual size is " << n << "!\n";
                errors += 1;
            }
            tree->clear();
            if (!tree->empty()) {
                log << "Expected empty tree after clear, actual size is " << tree->size() << "!\n";
                errors += 1;
            }
            tree->assign( copy );
            n = tree->size();
            if (n != content.size()) {
                log << "Expected size after re-assignment is " << content.size() << ", actual size is " << n << "!\n";
                errors += 1;
            }
        }
        delete temp;
        return errors;
    };
}; // class Uint32Uint32TreeTester

class Uint16ArrayUint32TreeTester : public TreeTester< uint16_t[], uint32_t > {
protected:
    Uint16ArrayCompare compare;
    vector<vector<uint16_t>*> keys;
    map<vector<uint16_t>*,uint32_t,Uint16ArrayCompare> content;
    map<vector<uint16_t>*,uint32_t,Uint16ArrayCompare> commitedContent;
private:
    vector<uint16_t>* generateUniqueKey() const {
        auto key = new vector<uint16_t>;
        generateUint16Array( *key );
        while (content.count( key ) == 1) generateUint16Array( *key );
        return( key );
    };
    vector<vector<uint16_t>*>* generateUniqueKeys( uint32_t count ) {
        auto keys = new vector<vector<uint16_t>*>();
        set<vector<uint16_t>*,Uint16ArrayCompare> keySet;
        times( count ) {
            auto key = new vector<uint16_t>;
            generateUint16Array( *key );
            while ((keySet.count( key ) == 1) || (content.count( key ) == 1)) generateUint16Array( *key );
            keys->push_back( key );
            keySet.insert( key );
        }
        return keys;
    }
    uint32_t insertKey( vector<uint16_t>* key, bool* usePair ) {
        uint32_t errors = 0;
        uint32_t value = generateUint32();
        bool inserted = false;
        if (*usePair) {
            pair<const uint16_t*, PageSize> keyPair = { key->data(), static_cast<PageSize>(key->size()) };
            inserted = tree->insert( keyPair, value );
        } else {
            inserted = tree->insert( key->data(), static_cast<PageSize>(key->size()), value );
        }
        if (!inserted) {
            log << "Insert on non-existing key "; logUint16Array( log, key->data(), static_cast<PageSize>(key->size()) ); log << " returned false!\n";
            errors += 1;
        } else {
            keys.push_back( key );
            content[ key ] = value;
        }
        return errors;
    }
    uint32_t removeKey( vector<uint16_t>* key, bool* usePair ) {
        uint32_t errors = 0;
        bool removed = false;
        if (*usePair) {
            pair<const uint16_t*, PageSize> keyPair = { key->data(), PageSize( static_cast<PageSize>(key->size()) ) };
            removed = tree->erase( keyPair );
        } else {
            removed = tree->erase( key->data(), PageSize( static_cast<PageSize>(key->size()) ));
        }
        if (!removed) {
            log << "Remove with existing key "; logUint16Array( log, key->data(), static_cast<PageSize>(key->size()) ); log << " returned false!\n";
            errors += 1;
        } else {
            content.erase( key );        
        }
        *usePair = !(*usePair);
        return errors;
    }
public:
    Uint16ArrayUint32TreeTester() = delete;
    Uint16ArrayUint32TreeTester( const string dir, const string file, ostream& logStream ) :
        TreeTester<uint16_t[],uint32_t>( dir, file, logStream )
    {}
    void createTree() {
        TreeTester<uint16_t[],uint32_t>::createTree();
        for (auto entry = commitedContent.cbegin(); entry != commitedContent.cend(); ++entry) {
            content[ new vector<uint16_t>( *entry->first ) ] = entry->second;
        }
        for (auto entry = content.cbegin(); entry != content.cend(); ++entry) keys.push_back( const_cast<vector<uint16_t>*>(entry->first) );
    }
    void destroyTree() {
        TreeTester<uint16_t[],uint32_t>::destroyTree();
        for (auto entry = content.cbegin(); entry != content.cend(); ++entry) {
             delete entry->first;
        }
        keys.clear();
        content.clear();
    }
    void deletePersistentStore() {
        TreeTester<uint16_t[],uint32_t>::deletePersistentStore();
        for (auto entry = commitedContent.cbegin(); entry != commitedContent.cend(); ++entry) {
            delete entry->first;
        }
        commitedContent.clear();
    }
    size_t size() const { return content.size(); }
    uint32_t validate() const {
        BTreeStatistics stats;
        bool statsEnabled = ((tree != nullptr) && tree->disableStatistics( &stats ));
        uint32_t errors = TreeTester<uint16_t[],uint32_t>::validate();
        if (tree != nullptr) {
            bool usePair = false;
            // Ensure that all expected content is actually present
            for (auto entry = content.cbegin(); entry != content.cend(); ++entry) {
                try {
                    uint32_t retrieved;
                    if (usePair) {
                        pair<const uint16_t*, PageSize> keyPair = { entry->first->data(), static_cast<PageSize>(entry->first->size()) };
                        retrieved = tree->at( keyPair );
                    } else {
                        retrieved = tree->at( entry->first->data(), static_cast<PageSize>(entry->first->size()) );
                    }
                    if (retrieved != entry->second) {
                        log << "Key " << entry->first << " : Expected " << entry->second << ", retrieved " << retrieved << "!\n";
                        errors += 1;
                    }
                }
                catch ( string message ) {
                    log << "Exception : " << message << "!\n";
                    errors += 1;
                }
                catch (...) {
                    log << "Exception (...)!\n";
                    errors += 1;
                }
                bool found = false;
                if (usePair) {
                    pair<const uint16_t*, PageSize> keyPair = { entry->first->data(), static_cast<PageSize>(entry->first->size()) };
                    found = tree->contains( keyPair );
                } else {
                    found = tree->contains( entry->first->data(), static_cast<PageSize>(static_cast<PageSize>(entry->first->size())) );
                }
                if (!found) {
                    log << "Contains with existing key ";
                    logUint16Array( log, entry->first->data(), static_cast<PageSize>(entry->first->size()) );
                    log << " returned false!\n";
                    errors += 1;
                }
                PageSize count = 0;
                if (usePair) {
                    pair<const uint16_t*, PageSize> keyPair = { entry->first->data(), static_cast<PageSize>(entry->first->size()) };
                    count = tree->count( keyPair );
                } else {
                    count = tree->count( entry->first->data(), static_cast<PageSize>(entry->first->size()) );
                }
                if (count != 1) {
                    log << "Count with existing key ";
                    logUint16Array( log, entry->first->data(), static_cast<PageSize>(entry->first->size()) );
                    log << " returned " << count << "!\n";
                    errors += 1;
                }
                usePair = !usePair;
            }

            // Ensure that other content is not present
            if (TryUnexpectedKeys) log << "Trying un-expected keys...\n";
            times( ProbeCount ) {
                vector<uint16_t>* key = generateUniqueKey();
                if (TryUnexpectedKeys) {
                    try {
                        uint32_t retrieved;
                        if (usePair) {
                            pair<const uint16_t*, PageSize> keyPair = { key->data(), static_cast<PageSize>(key->size()) };
                            retrieved = tree->at( keyPair );
                        } else {
                            retrieved = tree->at( key->data(), static_cast<PageSize>(key->size()) );
                        }
                        log << "Retrieved " << retrieved << " with unexpected key ";
                        logUint16Array( log, key->data(), static_cast<PageSize>(key->size()) );
                        log << "!\n";
                        errors += 1;
                    }
                    catch (...) {
                        // Exception expected, ignore
                    }
                }
                bool found = false;
                if (usePair) {
                    pair<const uint16_t*, PageSize> keyPair = { key->data(), static_cast<PageSize>(key->size()) };
                    found = tree->contains( keyPair );
                } else {
                    found = tree->contains( key->data(), static_cast<PageSize>(key->size()) );
                }
                if (found) {
                    log << "Contains with non-existing key ";
                    logUint16Array( log, key->data(), static_cast<PageSize>(key->size()) );
                    log << " returned true!\n";
                    errors += 1;
                }
                PageSize count = 0;
                if (usePair) {
                    pair<const uint16_t*, PageSize> keyPair = { key->data(), static_cast<PageSize>(key->size()) };
                    count = tree->count( keyPair );
                } else {
                    count = tree->count( key->data(), static_cast<PageSize>(key->size()) );
                }
                if (count != 0) {
                    log << "Count with non-existing key ";
                    logUint16Array( log, key->data(), static_cast<PageSize>(key->size()) );
                    log << " returned " << count << "!\n";
                    errors += 1;
                }
                delete key;
                usePair = !usePair;
            }
        }
        if (statsEnabled) tree->enableStatistics( &stats );
        return errors;
    }
    uint32_t insert( int count, KeyOrder order ) {
        uint32_t errors = TreeTester<uint16_t[],uint32_t>::insert( count, order );
        bool usePair = false;
        auto insertKeys = generateUniqueKeys( count );
        if (order == Forward) {
            sort( insertKeys->begin(), insertKeys->end() );
            for ( auto key : *insertKeys ) { errors += insertKey( key, &usePair ); }
        } else if (order == Reverse) {
            sort( insertKeys->rbegin(), insertKeys->rend() );
            for ( auto key : *insertKeys ) { errors += insertKey( key, &usePair ); }
        } else if (order == Random) {
            shuffle( insertKeys->begin(), insertKeys->end(), gen32 );
            for ( auto key : *insertKeys ) { errors += insertKey( key, &usePair ); }
        }
        delete insertKeys;
        for( auto entry : content ) {
            bool inserted = false;
            if (usePair) {
                pair<const uint16_t*, PageSize> keyPair = { entry.first->data(), PageSize( static_cast<PageSize>(entry.first->size()) ) };
                inserted = tree->insert( keyPair, entry.second );
            } else {
                inserted = tree->insert( entry.first->data(), PageSize( static_cast<PageSize>(entry.first->size()) ), entry.second );
            }
            if (inserted) {
                log << "Insert with existing key ";
                logUint16Array( log, entry.first->data(), static_cast<PageSize>(entry.first->size()) );
                log << " returned true!\n";
                errors += 1;
            }
            usePair = !usePair;
        }
        if (0 < errors) log << "Detected " << errors << " insert errors.\n";
        return errors;
    };
    uint32_t replace( int count ) {
        uint32_t errors = TreeTester<uint16_t[],uint32_t>::replace( count );
        size_t range = keys.size();
        if (range < count) count = static_cast<int>(range);
        bool usePair = false;
        for( int i = 0; i < count; ++i) {
            vector<uint16_t>* key = keys[ generateIndex( count ) ];
            uint32_t value = generateUint32();
            bool replaced = false;
            if (usePair) {
                pair<const uint16_t*, PageSize> keyPair = { key->data(), PageSize( static_cast<PageSize>(key->size()) ) };
                replaced = tree->replace( keyPair, value );
            } else {
                replaced = tree->replace( key->data(), PageSize( static_cast<PageSize>(key->size()) ), value );
            }
            if (!replaced) {
                log << "Replace with existing key ";
                logUint16Array( log, key->data(), static_cast<PageSize>(key->size()) );
                log << " returned false!\n";
                errors += 1;
            } else {
                content[ key ] = value;
            }
            usePair = !usePair;
        }
        times( ProbeCount ) {
            vector<uint16_t>* key = generateUniqueKey();
            bool replaced = false;
            if (usePair) {
                pair<const uint16_t*, PageSize> keyPair = { key->data(), PageSize( static_cast<PageSize>(key->size()) ) };
                replaced = tree->replace( keyPair, generateUint32() );
            } else {
                replaced = tree->replace( key->data(), PageSize( static_cast<PageSize>(key->size()) ), generateUint32() );
            }
            if (replaced) {
                log << "Replace with non-existing key ";
                logUint16Array( log, key->data(), static_cast<PageSize>(key->size()) );
                log << " returned true!\n";
                errors += 1;
            }
            delete key;
            usePair = !usePair;
        }
        if (0 < errors) log << "Detected " << errors << " replace errors.\n";
        return errors;
    }
    uint32_t erase( int count, KeyOrder order ) {
        uint32_t errors = TreeTester<uint16_t[],uint32_t>::erase( count, order );
        size_t range = keys.size();
        if (range < count) count = static_cast<int>(range);
        bool usePair = false;
        if (order == Forward) {
            sort( keys.begin(), keys.end(), compare );
            each( i, count ) { errors += removeKey( keys[ i ], &usePair ); }
            keys.erase( keys.begin(), keys.begin() + count );
        } else if (order == Reverse) {
            sort( keys.rbegin(), keys.rend(), compare );
            each( i, count ) { errors += removeKey( keys[ i ], &usePair ); }
            keys.erase( keys.end() - count, keys.end() );
        } else if (order == Random) {
            shuffle( keys.begin(), keys.end(), gen32 );
            each( i, count ) { errors += removeKey( keys[ i ], &usePair ); }
            keys.erase( keys.begin(), (keys.begin() + count) );
        }
        times( ProbeCount ) {
            vector<uint16_t>* key = generateUniqueKey();
            bool removed = false;
            if (usePair) {
                pair<const uint16_t*, PageSize> keyPair = { key->data(), PageSize( static_cast<PageSize>(key->size()) ) };
                removed = tree->erase( keyPair );
            } else {
                removed = tree->erase( key->data(), PageSize( static_cast<PageSize>(key->size()) ));
            }
            if (removed) {
                log << "Remove with non-existing key ";
                logUint16Array( log, key->data(), static_cast<PageSize>(key->size()) );
                log << " returned true!\n";
                errors += 1;
            }
            delete key;
            usePair = !usePair;
        }
        if (0 < errors) log << "Detected " << errors << " erase errors.\n";
        return errors;
    }
    uint32_t commit() {
        uint32_t errors = TreeTester<uint16_t[],uint32_t>::commit();
        for (auto entry = commitedContent.cbegin(); entry != commitedContent.cend(); ++entry) {
            delete entry->first;
        }
        commitedContent.clear();
        for (auto entry = content.cbegin(); entry != content.cend(); ++entry) {
            commitedContent[ new vector<uint16_t>( *entry->first ) ] = entry->second;
        }
        return errors;
    };
    uint32_t recover() {
        uint32_t errors = TreeTester<uint16_t[],uint32_t>::recover();
        keys.clear();
        for (auto entry = content.cbegin(); entry != content.cend(); ++entry) {
            delete entry->first;
        }
        content.clear();
        for (auto entry = commitedContent.cbegin(); entry != commitedContent.cend(); ++entry) {
            content[ new vector<uint16_t>( *entry->first ) ] = entry->second;
        }
        for (auto entry = content.cbegin(); entry != content.cend(); ++entry) keys.push_back( const_cast<vector<uint16_t>*>(entry->first) );
        return errors;
    };
    uint32_t assign() {
        uint32_t errors = TreeTester<uint16_t[],uint32_t>::assign();
        PagePool* temp = new PagePool( BTreePageSize * 2 );
        {
            // Enclose in block to destruct B-Tree prior to destructing
            // dynamically allocated PagePool
            Tree<uint16_t[],uint32_t> copy( *temp );
            copy.assign( *tree );
            size_t n = copy.size();
            if (n != content.size()) {
                log << "Expected size after assignment is " << content.size() << ", actual size is " << n << "!\n";
                errors += 1;
            }
            tree->clear();
            if (!tree->empty()) {
                log << "Expected empty tree after clear, actual size is " << tree->size() << "!\n";
                errors += 1;
            }
            tree->assign( copy );
            n = tree->size();
            if (n != content.size()) {
                log << "Expected size after re-assignment is " << content.size() << ", actual size is " << n << "!\n";
                errors += 1;
            }
        }
        delete temp;
        return errors;
    };
}; // class Uint16ArrayUint32TreeTester

class Uint32Uint16ArrayTreeTester : public TreeTester< uint32_t, uint16_t[] > {
protected:
    vector<uint32_t> keys;
    map<uint32_t,vector<uint16_t>*> content;
    map<uint32_t,vector<uint16_t>*> commitedContent;
private:
    uint32_t generateUniqueKey() const {
        uint32_t key = generateUint32();
        while (content.count( key ) == 1) key = generateUint32();
        return key;
    };
    vector<uint32_t>* generateUniqueKeys( uint32_t count ) const {
        auto keys = new vector<uint32_t>();
        set<uint32_t> keySet;
        times( count ) {
            uint32_t key = generateUint32();
            while ((keySet.count( key ) == 1) || (content.count( key ) == 1)) key = generateUint32();
            keys->push_back( key );
            keySet.insert( key );
        }
        return keys;
    }
    vector<uint16_t>* generateValue() const {
        auto value = new vector<uint16_t>;
        generateUint16Array( *value );
        return( value );
    };
    uint32_t insertKey( uint32_t key, bool* usePair ) {
        uint32_t errors = 0;
        auto value = generateValue();
        bool inserted = false;
        if (*usePair) {
            pair<const uint16_t*, PageSize> valuePair = { value->data(), PageSize( value->size() ) };
            inserted = tree->insert( key, valuePair );
        } else {
            inserted = tree->insert( key, value->data(), PageSize( value->size() ) );
        }
        if (!inserted) {
            log << "Insert on non-existing key " << key << " returned false!\n";
            errors += 1;
        } else {
            keys.push_back( key );
            content[ key ] = value;
        }
        *usePair = !(*usePair);
        return errors;
    }
    uint32_t removeKey( uint32_t key ) {
        uint32_t errors = 0;
        bool removed = tree->erase( key );
        if (!removed) {
            log << "Remove with existing key " << key << " returned false!\n";
            errors += 1;
        } else {
            content.erase( key );
        }
        return errors;
    }
public:
    Uint32Uint16ArrayTreeTester() = delete;
    Uint32Uint16ArrayTreeTester( const string dir, const string file, ostream& logStream ) :
        TreeTester( dir, file, logStream )
    {}
    void createTree() {
        TreeTester<uint32_t,uint16_t[]>::createTree();
        for (auto entry = commitedContent.cbegin(); entry != commitedContent.cend(); ++entry) {
            content[ entry->first ] = new vector<uint16_t>( *entry->second );
        }
        keys.clear();
        for (auto entry = content.cbegin(); entry != content.cend(); ++entry) keys.push_back( entry->first );
    }
    void destroyTree() {
        TreeTester<uint32_t,uint16_t[]>::destroyTree();
        for (auto entry = content.cbegin(); entry != content.cend(); ++entry) {
            delete entry->second;
        }
        keys.clear();
        content.clear();
    }
    void deletePersistentStore() {
        TreeTester<uint32_t,uint16_t[]>::deletePersistentStore();
        for (auto entry = commitedContent.cbegin(); entry != commitedContent.cend(); ++entry) {
            delete entry->second;
        }
        commitedContent.clear();
    }
    size_t size() const { return content.size(); }
    uint32_t validate() const {
        BTreeStatistics stats;
        bool statsEnabled = ((tree != nullptr) && tree->disableStatistics( &stats ));
        uint32_t errors = TreeTester<uint32_t,uint16_t[]>::validate();
        if (tree != nullptr) {
            // Ensure that all expected content is actually present
            for (auto entry = content.cbegin(); entry != content.cend(); ++entry) {
                try {
                    auto retrieved = tree->at( entry->first );
                    if (Uint16ArrayCompare::compare( entry->second->data(), static_cast<PageSize>(entry->second->size()), retrieved.first, retrieved.second )) {
                        log << "Key " << entry->first << " : Expected ";
                        logUint16Array( log, entry->second->data(), static_cast<PageSize>(entry->second->size()) );
                        log << ", retrieved ";
                        logUint16Array( log, retrieved.first, retrieved.second );
                        log << "!\n";
                        errors += 1;
                    }
                }
                catch ( string message ) {
                    log << "Exception : " << message << "!\n";
                    errors += 1;
                }
                catch (...) {
                    log << "Exception (...)!\n";
                    errors += 1;
                }
                bool found = tree->contains( entry->first );
                if (!found) {
                    log << "Contains with existing key " << entry->first << " return false!\n";
                    errors += 1;
                }
                PageSize count = tree->count( entry->first );
                if (count != 1) {
                    log << "Count with existing key " << entry->first << " returned " << count << "!\n";
                    errors += 1;
                }
            }
            // Ensure that other content is not present
            if (TryUnexpectedKeys) log << "Trying un-expected keys...\n";
            times( ProbeCount ) {
                uint32_t key = generateUniqueKey();
                if (TryUnexpectedKeys) {
                    try {
                        auto retrieved = tree->at( key );
                        log << "Retrieved ";
                        logUint16Array( log, retrieved.first, retrieved.second );
                        log << " with unexpected key " << key << "!\n";
                        errors += 1;
                    }
                    catch (...) {
                        // Exception expected, ignore
                    }
                }
                bool found = tree->contains( key );
                if (found) {
                    log << "Contains with non-existing key " << key << " returned true!\n";
                    errors += 1;
                }
                PageSize count = tree->count( key );
                if (count != 0) {
                    log << "Count with non-existing key " << key << " returned " << count << "!\n";
                    errors += 1;
                }
            }
        }
        if (statsEnabled) tree->enableStatistics( &stats );
        return errors;
    }
    uint32_t insert( int count, KeyOrder order ) {
        uint32_t errors = TreeTester<uint32_t,uint16_t[]>::insert( count, order );
        bool usePair = false;
        vector<uint32_t>* insertKeys = generateUniqueKeys( count );
        if (order == Forward) {
            sort( insertKeys->begin(), insertKeys->end() );
            for ( auto key : *insertKeys ) { errors += insertKey( key, &usePair ); }
        } else if (order == Reverse) {
            sort( insertKeys->rbegin(), insertKeys->rend() );
            for ( auto key : *insertKeys ) { errors += insertKey( key, &usePair ); }
        } else if (order == Random) {
            shuffle( insertKeys->begin(), insertKeys->end(), gen32 );
            for ( auto key : *insertKeys ) { errors += insertKey( key, &usePair ); }
        }
        delete insertKeys;
        for( auto entry : content ) {
            bool inserted = false;
            if (usePair) {
                pair<const uint16_t*, PageSize> valuePair = { entry.second->data(), PageSize( entry.second->size() ) };
                inserted = tree->insert( entry.first, valuePair );
            } else {
                inserted = tree->insert( entry.first, entry.second->data(), PageSize( entry.second->size() ) );
            }
            if (inserted) {
                log << "Insert on existing key " << entry.first << " returned true!\n";
                errors += 1;
            }
            usePair = !usePair;
        }
        if (0 < errors) log << "Detected " << errors << " insert errors!\n";
        return errors;
    };
    uint32_t replace( int count ) {
        uint32_t errors = TreeTester<uint32_t,uint16_t[]>::replace( count );
        size_t range = keys.size();
        if (range < count) count = static_cast<int>(range);
        bool usePair = false;
        for( int i = 0; i < count; ++i) {
            uint32_t key = keys[ generateIndex( range ) ];
            vector<uint16_t>* value = generateValue();
            bool replaced = false;
            if (usePair) {
                pair<const uint16_t*, PageSize> valuePair = { value->data(), PageSize( value->size() ) };
                replaced = tree->replace( key, valuePair );
            } else {
                replaced = tree->replace( key, value->data(), PageSize( value->size() ) );
                
            }
            if (!replaced) {
                log << "Replace with existing key " << key << " returned false!\n";
            } else {
                vector<uint16_t>* oldValue = content[ key ];
                content[ key ] = value;
                delete oldValue;
            }
            usePair = !usePair;
        }
        times( ProbeCount ) {
            uint32_t key = generateUniqueKey();
            vector<uint16_t>* value = generateValue();
            bool replaced = false;
            if (usePair) {
                pair<const uint16_t*, PageSize> valuePair = { value->data(), PageSize( value->size() ) };
                replaced = tree->replace( key, valuePair );
            } else {
                replaced = tree->replace( key, value->data(), PageSize( value->size() ));
            }
            if (replaced) {
                log << "Replace with non-existing key " << key << " returned true!\n";
                errors += 1;
            }
            delete value;
            usePair = !usePair;
        }
        if (0 < errors) log << "Detected " << errors << " replace errors!\n";
        return errors;
    }

    uint32_t erase( int count, KeyOrder order ) {
        uint32_t errors = TreeTester<uint32_t,uint16_t[]>::erase( count, order );
        size_t range = keys.size();
        if (range < count) count = static_cast<int>(range);
        if (order == Forward) {
            sort( keys.begin(), keys.end() );
            each( i, count ) errors += removeKey( keys[ i ] );
            keys.erase( keys.begin(), keys.begin() + count );
        } else if (order == Reverse) {
            sort( keys.rbegin(), keys.rend() );
            each( i, count ) errors += removeKey( keys[ i ] );
            keys.erase( keys.end() - count, keys.end() );
        } else if (order == Random) {
            shuffle( keys.begin(), keys.end(), gen32 );
            each( i, count ) errors += removeKey( keys[ i ] );
            keys.erase( keys.begin(), (keys.begin() + count) );
        }
        times( ProbeCount ) {
            uint32_t key = generateUniqueKey();
            bool removed = tree->erase( key );
            if (removed) {
                log << "Remove with non-existing key " << key << " returned true!\n";
                errors += 1;
            }
        }
        if (0 < errors) log << "Detected " << errors << " erase errors!\n";
        return errors;
    }
    uint32_t commit() {
        uint32_t errors = TreeTester<uint32_t,uint16_t[]>::commit();
        for (auto entry = commitedContent.cbegin(); entry != commitedContent.cend(); ++entry) {
            delete entry->second;
        }
        commitedContent.clear();
        for (auto entry = content.cbegin(); entry != content.cend(); ++entry) {
            commitedContent[ entry->first ] = new vector<uint16_t>( *entry->second );
        }
        return errors;
    };
    uint32_t recover() {
        uint32_t errors = TreeTester<uint32_t,uint16_t[]>::recover();
        keys.clear();
        for (auto entry = content.cbegin(); entry != content.cend(); ++entry) {
            delete entry->second;
        }
        content.clear();
        for (auto entry = commitedContent.cbegin(); entry != commitedContent.cend(); ++entry) {
            content[ entry->first ] = new vector<uint16_t>( *entry->second );
        }
        for (auto entry = content.cbegin(); entry != content.cend(); ++entry) keys.push_back( entry->first );
        return errors;
    };
    uint32_t assign() {
        uint32_t errors = TreeTester<uint32_t,uint16_t[]>::assign();
        PagePool* temp = new PagePool( BTreePageSize * 2 );
        {
            // Enclose in block to destruct B-Tree prior to destructing
            // dynamically allocated PagePool
            Tree<uint32_t,uint16_t[]> copy( *temp );
            copy.assign( *tree );
            size_t n = copy.size();
            if (n != content.size()) {
                log << "Expected size after assignment is " << content.size() << ", actual size is " << n << "!\n";
                errors += 1;
            }
            tree->clear();
            if (!tree->empty()) {
                log << "Expected empty tree after clear, actual size is " << tree->size() << "!\n";
                errors += 1;
            }
            tree->assign( copy );
            n = tree->size();
            if (n != content.size()) {
                log << "Expected size after re-assignment is " << content.size() << ", actual size is " << n << "!\n";
                errors += 1;
            }
        }
        delete temp;
        return errors;
    };
}; // class Uint32Uint16ArrayTreeTester

class Uint16ArrayUint16ArrayTreeTester : public TreeTester< uint16_t[], uint16_t[] > {
protected:
    Uint16ArrayCompare compare;
    vector<vector<uint16_t>*> keys;
    map<vector<uint16_t>*,vector<uint16_t>*,Uint16ArrayCompare> content;
    map<vector<uint16_t>*,vector<uint16_t>*,Uint16ArrayCompare> commitedContent;
private:
    vector<uint16_t>* generateUniqueKey() const {
        auto key = new vector<uint16_t>;
        generateUint16Array( *key );
        while (content.count( key ) == 1) generateUint16Array( *key );
        return( key );
    };
    vector<vector<uint16_t>*>* generateUniqueKeys( uint32_t count ) {
        auto keys = new vector<vector<uint16_t>*>();
        set<vector<uint16_t>*,Uint16ArrayCompare> keySet;
        times( count ) {
            auto key = new vector<uint16_t>;
            generateUint16Array( *key );
            while ((keySet.count( key ) == 1) || (content.count( key ) == 1)) generateUint16Array( *key );
            keys->push_back( key );
            keySet.insert( key );
        }
        return keys;
    }
    vector<uint16_t>* generateValue() const {
        auto value = new vector<uint16_t>;
        generateUint16Array( *value );
        return( value );
    };
    uint32_t insertKey( vector<uint16_t>* key, bool* usePair ) {
        uint32_t errors = 0;
        auto value = generateValue();
        bool inserted = false;
        if (*usePair) {
            pair<const uint16_t*, PageSize> keyPair = { key->data(), static_cast<PageSize>(key->size()) };
            pair<const uint16_t*, PageSize> valuePair = { value->data(), static_cast<PageSize>(value->size()) };
            inserted = tree->insert( keyPair, valuePair );
        } else {
            inserted = tree->insert( key->data(), static_cast<PageSize>(key->size()), value->data(), static_cast<PageSize>(value->size()) );
        }
        if (!inserted) {
            log << "Insert on non-existing key "; logUint16Array( log, key->data(), static_cast<PageSize>(key->size()) ); log << " returned false!\n";
            errors += 1;
        } else {
            keys.push_back( key );
            content[ key ] = value;
        }
        return errors;
    }
    uint32_t removeKey( vector<uint16_t>* key, bool* usePair ) {
        uint32_t errors = 0;
        bool removed = false;
        if (*usePair) {
            pair<const uint16_t*, PageSize> keyPair = { key->data(), PageSize( static_cast<PageSize>(key->size()) ) };
            removed = tree->erase( keyPair );
        } else {
            removed = tree->erase( key->data(), PageSize( static_cast<PageSize>(key->size()) ));
        }
        if (!removed) {
            log << "Remove with existing key "; logUint16Array( log, key->data(), static_cast<PageSize>(key->size()) ); log << " returned false!\n";
            errors += 1;
        } else {
            content.erase( key );
        }
        *usePair = !(*usePair);
        return errors;
    }
public:
    Uint16ArrayUint16ArrayTreeTester() = delete;
    Uint16ArrayUint16ArrayTreeTester( const string dir, const string file, ostream& logStream ) :
        TreeTester<uint16_t[],uint16_t[]>( dir, file, logStream )
    {}
    void createTree() {
        TreeTester<uint16_t[],uint16_t[]>::createTree();
        for (auto entry = commitedContent.cbegin(); entry != commitedContent.cend(); ++entry) {
            content[ new vector<uint16_t>( *entry->first ) ] = new vector<uint16_t>( *entry->second );
        }
        for (auto entry = content.cbegin(); entry != content.cend(); ++entry) keys.push_back( const_cast<vector<uint16_t>*>(entry->first) );
    }
    void destroyTree() {
        TreeTester<uint16_t[],uint16_t[]>::destroyTree();
        for (auto entry = content.cbegin(); entry != content.cend(); ++entry) {
            delete entry->first;
            delete entry->second;
        }
        keys.clear();
        content.clear();
    }
    void deletePersistentStore() {
        TreeTester<uint16_t[],uint16_t[]>::deletePersistentStore();
        for (auto entry = commitedContent.cbegin(); entry != commitedContent.cend(); ++entry) {
            delete entry->first;
            delete entry->second;
        }
        commitedContent.clear();
    }
    size_t size() const { return content.size(); }
    uint32_t validate() const {
        BTreeStatistics stats;
        bool statsEnabled = ((tree != nullptr) && tree->disableStatistics( &stats ));
        uint32_t errors = TreeTester<uint16_t[],uint16_t[]>::validate();
        if (tree != nullptr) {
            // Ensure that all expected content is actually present...
            bool usePair = false;
            for (auto entry = content.cbegin(); entry != content.cend(); ++entry) {
                try {
                    pair<const uint16_t*,PageSize> retrieved;
                    if (usePair) {
                        pair<const uint16_t*, PageSize> keyPair = { entry->first->data(), static_cast<PageSize>(entry->first->size()) };
                        retrieved = tree->at( keyPair );
                    } else {
                        retrieved = tree->at( entry->first->data(), static_cast<PageSize>(entry->first->size()) );
                    }
                    if (Uint16ArrayCompare::compare( entry->second->data(), static_cast<PageSize>(entry->second->size()), retrieved.first, retrieved.second )) {
                        log << "Key ";
                        logUint16Array( log, entry->first->data(), static_cast<PageSize>(entry->first->size()) );
                        log << " : Expected ";
                        logUint16Array( log, entry->second->data(), static_cast<PageSize>(entry->second->size()) );
                        log << ", retrieved ";
                        logUint16Array( log, retrieved.first, retrieved.second );
                        log << ".\n";
                        errors += 1;
                    }
                }
                catch ( string message ) {
                    log << "Exception : " << message << "!\n";
                    errors += 1;
                }
                catch (...) {
                    log << "Exception (...)!\n";
                    errors += 1;
                }
                bool found = false;
                if (usePair) {
                    pair<const uint16_t*, PageSize> keyPair = { entry->first->data(), static_cast<PageSize>(entry->first->size()) };
                    found = tree->contains( keyPair );
                } else {
                    found = tree->contains( entry->first->data(), static_cast<PageSize>(entry->first->size()) );
                }
                if (!found) {
                    log << "Contains with existing key ";
                    logUint16Array( log, entry->first->data(), static_cast<PageSize>(entry->first->size()) );
                    log << " returned false!\n";
                    errors += 1;
                }
                PageSize count = 0;
                if (usePair) {
                    pair<const uint16_t*, PageSize> keyPair = { entry->first->data(), static_cast<PageSize>(entry->first->size()) };
                    count = tree->count( keyPair );
                } else {
                    count = tree->count( entry->first->data(), static_cast<PageSize>(entry->first->size()) );
                }
                if (count != 1) {
                    log << "Count with existing key ";
                    logUint16Array( log, entry->first->data(), static_cast<PageSize>(entry->first->size()) );
                    log << " returned " << count << "!\n";
                    errors += 1;
                }
                usePair = !usePair;
            }
            // Ensure that other content is not present
            if (TryUnexpectedKeys) log << "Trying un-expected keys...\n";
            times( ProbeCount ) {
                vector<uint16_t>* key = generateUniqueKey();
                if (TryUnexpectedKeys) {
                    try {
                        pair<const uint16_t*,PageSize> retrieved;
                        if (usePair) {
                            pair<const uint16_t*, PageSize> keyPair = { key->data(), static_cast<PageSize>(key->size()) };
                            retrieved = tree->at( keyPair );
                        } else {
                            retrieved = tree->at( key->data(), static_cast<PageSize>(key->size()) );
                        }
                        log << "Retrieved ";
                        logUint16Array( log, retrieved.first, retrieved.second );
                        log << " with non-existing key ";
                        logUint16Array( log, key->data(), static_cast<PageSize>(key->size()) );
                        log << "!\n";
                        errors += 1;
                    }
                    catch (...) {
                        // Exception expected, ignore
                    }
                }
                bool found = false;
                if (usePair) {
                    pair<const uint16_t*, PageSize> keyPair = { key->data(), static_cast<PageSize>(key->size()) };
                    found = tree->contains( keyPair );
                } else {
                    found = tree->contains( key->data(), static_cast<PageSize>(key->size()) );
                }
                if (found) {
                    log << "Contains with non-existing key ";
                    logUint16Array( log, key->data(), static_cast<PageSize>(key->size()) );
                    log << " returned true!\n";
                    errors += 1;
                }
                PageSize count = 0;
                if (usePair) {
                    pair<const uint16_t*, PageSize> keyPair = { key->data(), static_cast<PageSize>(key->size()) };
                    count = tree->count( keyPair );
                } else {
                    count = tree->count( key->data(), static_cast<PageSize>(key->size()) );
                }
                if (count != 0) {
                    log << "Count with non-existing key ";
                    logUint16Array( log, key->data(), static_cast<PageSize>(key->size()) );
                    log << " returned " << count << "!\n";
                    errors += 1;
                }
                delete key;
                usePair = !usePair;
            }
        }
        if (statsEnabled) tree->enableStatistics( &stats );
        return errors;
    }
    uint32_t insert( int count, KeyOrder order ) {
        uint32_t errors = TreeTester<uint16_t[],uint16_t[]>::insert( count, order );
        bool usePair = false;
        bool inserted = false;
        auto insertKeys = generateUniqueKeys( count );
        if (order == Forward) {
            sort( insertKeys->begin(), insertKeys->end() );
            for ( auto key : *insertKeys ) { errors += insertKey( key, &usePair ); }
        } else if (order == Reverse) {
            sort( insertKeys->rbegin(), insertKeys->rend() );
            for ( auto key : *insertKeys ) { errors += insertKey( key, &usePair ); }
        } else if (order == Random) {
            shuffle( insertKeys->begin(), insertKeys->end(), gen32 );
            for ( auto key : *insertKeys ) { errors += insertKey( key, &usePair ); }
        }
        delete insertKeys;
        for( auto entry : content ) {
            bool inserted = false;
            if (usePair) {
                pair<const uint16_t*, PageSize> keyPair = { entry.first->data(), PageSize( static_cast<PageSize>(entry.first->size()) ) };
                pair<const uint16_t*, PageSize> valuePair = { entry.second->data(), PageSize( entry.second->size() ) };
                inserted = tree->insert( keyPair, valuePair );
            } else {
                inserted = tree->insert( entry.first->data(), PageSize( static_cast<PageSize>(entry.first->size()) ), entry.second->data(), PageSize( entry.second->size() ) );
            }
            if (inserted) {
                log << "Insert on existing key ";
                logUint16Array( log, entry.first->data(), static_cast<PageSize>(entry.first->size()) );
                log << " returned true!\n";
                errors += 1;
            }
            usePair = !usePair;
        }
        if (0 < errors) log << "Detected " << errors << " insert errors!\n";
        return errors;
    };
    uint32_t replace( int count ) {
        uint32_t errors = TreeTester<uint16_t[],uint16_t[]>::replace( count );
        size_t range = keys.size();
        if (range < count) count = static_cast<int>(range);
        bool usePair = false;
        for( int i = 0; i < count; ++i) {
            vector<uint16_t>* key = keys[ generateIndex( range ) ];
            vector<uint16_t>* value = generateValue();
            bool replaced = false;
            if (usePair) {
                pair<const uint16_t*, PageSize> keyPair = { key->data(), PageSize( static_cast<PageSize>(key->size()) ) };
                pair<const uint16_t*, PageSize> valuePair = { value->data(), PageSize( value->size() ) };
                replaced = tree->replace( keyPair, valuePair );
            } else {
                replaced = tree->replace( key->data(), PageSize( static_cast<PageSize>(key->size()) ), value->data(), PageSize( value->size() ) );
            }
            if (!replaced) {
                log << "Replace with existing key ";
                logUint16Array( log, key->data(), static_cast<PageSize>(key->size()) );
                log << " returned false!\n";
                errors += 1;
            } else {
                vector<uint16_t>* oldValue = content[ key ];
                content[ key ] = value;
                delete oldValue;
            }
            usePair = !usePair;
        }
        times( ProbeCount ) {
            vector<uint16_t>* key = generateUniqueKey();
            vector<uint16_t>* value = generateValue();
            bool replaced = false;
            if (usePair) {
                pair<const uint16_t*, PageSize> keyPair = { key->data(), PageSize( static_cast<PageSize>(key->size()) ) };
                pair<const uint16_t*, PageSize> valuePair = { value->data(), PageSize( value->size() ) };
                replaced = tree->replace( keyPair, valuePair );
            } else {
                replaced = tree->replace( key->data(), PageSize( static_cast<PageSize>(key->size()) ), value->data(), PageSize( value->size() ));
            }
            if (replaced) {
                log << "Replace with non-existing key ";
                logUint16Array( log, key->data(), static_cast<PageSize>(key->size()) );
                log << " returned true!\n";
                errors += 1;
            }
            delete key;
            delete value;
            usePair = !usePair;
        }
        if (0 < errors) log << "Detected " << errors << " replace errors!\n";
        return errors;
    }

    uint32_t erase( int count, KeyOrder order ) {
        uint32_t errors = TreeTester<uint16_t[],uint16_t[]>::erase( count, order );
        size_t range = keys.size();
        if (range < count) count = static_cast<int>(range);
        bool usePair = false;
        if (order == Forward) {
            sort( keys.begin(), keys.end(), compare );
            each( i, count ) { errors += removeKey( keys[ i ], &usePair ); }
            keys.erase( keys.begin(), keys.begin() + count );
        } else if (order == Reverse) {
            sort( keys.rbegin(), keys.rend(), compare );
            each( i, count ) { errors += removeKey( keys[ i ], &usePair ); }
            keys.erase( keys.end() - count, keys.end() );
        } else if (order == Random) {
            shuffle( keys.begin(), keys.end(), gen32 );
            each( i, count ) { errors += removeKey( keys[ i ], &usePair ); }
            keys.erase( keys.begin(), (keys.begin() + count) );
        }
        times( ProbeCount ) {
            vector<uint16_t>* key = generateUniqueKey();
            bool removed = false;
            if (usePair) {
                pair<const uint16_t*, PageSize> keyPair = { key->data(), PageSize( static_cast<PageSize>(key->size()) ) };
                removed = tree->erase( keyPair );
            } else {
                removed = tree->erase( key->data(), PageSize( static_cast<PageSize>(key->size()) ));
            }
            if (removed) {
                log << "Remove with non-existing key ";
                logUint16Array( log, key->data(), static_cast<PageSize>(key->size()) );
                log << " returned true!\n";
                errors += 1;
            }
            delete key;
            usePair = !usePair;
        }
        if (0 < errors) log << "Detected " << errors << " erase errors!\n";
        return errors;
    }
    uint32_t commit() {
        uint32_t errors = TreeTester<uint16_t[],uint16_t[]>::commit();
        for (auto entry = commitedContent.cbegin(); entry != commitedContent.cend(); ++entry) {
            delete entry->first;
            delete entry->second;
        }
        commitedContent.clear();
        for (auto entry = content.cbegin(); entry != content.cend(); ++entry) {
            commitedContent[ new vector<uint16_t>( *entry->first ) ] = new vector<uint16_t>( *entry->second );
        }
        return errors;
    };
    uint32_t recover() {
        uint32_t errors = TreeTester<uint16_t[],uint16_t[]>::recover();
        keys.clear();
        for (auto entry = content.cbegin(); entry != content.cend(); ++entry) {
            delete entry->first;
            delete entry->second;
        }
        content.clear();
        for (auto entry = commitedContent.cbegin(); entry != commitedContent.cend(); ++entry) {
            content[ new vector<uint16_t>( *entry->first ) ] = new vector<uint16_t>( *entry->second );
        }
        for (auto entry = content.cbegin(); entry != content.cend(); ++entry) keys.push_back( const_cast<vector<uint16_t>*>(entry->first) );
        return errors;
    };
    uint32_t assign() {
        uint32_t errors = TreeTester<uint16_t[],uint16_t[]>::assign();
        PagePool* temp = new PagePool( BTreePageSize * 2 );
        {
            // Enclose in block to destruct B-Tree prior to destructing
            // dynamically allocated PagePool
            Tree<uint16_t[],uint16_t[]> copy( *temp );
            copy.assign( *tree );
            size_t n = copy.size();
            if (n != content.size()) {
                log << "Expected size after assignment is " << content.size() << ", actual size is " << n << "!\n";
                errors += 1;
            }
            tree->clear();
            if (!tree->empty()) {
                log << "Expected empty tree after clear, actual size is " << tree->size() << "!\n";
                errors += 1;
            }
            tree->assign( copy );
            n = tree->size();
            if (n != content.size()) {
                log << "Expected size after re-assignment is " << content.size() << ", actual size is " << n << "!\n";
                errors += 1;
            }
        }
        delete temp;
        return errors;
    };
    void statistics() const {
        TreeTester<uint16_t[],uint16_t[]>::statistics();
        auto treePages = tree->collectPages();
        vector<PageLink> freePages;
        vector<PageLink> modifiedPages;
        vector<PageLink> persistentPages;
        vector<PageLink> recoverPages;
        uint32_t pages = pool->size();
        for ( uint32_t index = 0; index < pages; ++index ) {
            PageLink link( index );
            auto pageHeader = pool->access( link );
            if (pageHeader.free == 1) freePages.push_back( link );
            if (pageHeader.modified == 1) modifiedPages.push_back( link );
            if (pageHeader.persistent == 1) persistentPages.push_back( link );
            if (pageHeader.recover == 1) recoverPages.push_back( link );
        }
        log << "Persistent page pool has " << pages << " pages"
            << ", B-Tree " << treePages->size()
            << ", free " << freePages.size()
            << ", modified " << modifiedPages.size()
            << ", persistent " << persistentPages.size()
            << ", recover " << recoverPages.size() << "\n";
        delete treePages;
    };

}; // class Uint16ArrayUint16ArrayTreeTester

template< class K, class V >
pair<uint32_t,bool> doTest( TreeTester<K,V>& tester, int count1, int count2, ofstream& log ) {
    uint32_t errors = 0;
    bool exception = false;
    try {
        log << "Forward, Reverse and Random insertion and removal test...\n";
        tester.persistent( false );
        tester.createPool();
        // Insertion in forward order
        tester.createTree();
        errors += tester.validate();
        errors += tester.validate();
        errors += tester.insert( count1, TreeTester<K,V>::Forward );
        errors += tester.validate();
        errors += tester.erase( count2, TreeTester<K,V>::Forward );
        errors += tester.validate();
        tester.logTree();
        tester.destroyTree();
        tester.createTree();
        errors += tester.insert( count1, TreeTester<K,V>::Forward );
        errors += tester.validate();
        errors += tester.erase( count2, TreeTester<K,V>::Reverse );
        errors += tester.validate();
        tester.logTree();
        tester.destroyTree();
        tester.createTree();
        errors += tester.insert( count1, TreeTester<K,V>::Forward );
        errors += tester.validate();
        errors += tester.erase( count2, TreeTester<K,V>::Random );
        errors += tester.validate();
        tester.logTree();
        tester.destroyTree();
        // Insertion in reverse order
        tester.createTree();
        errors += tester.validate();
        errors += tester.validate();
        errors += tester.insert( count1, TreeTester<K,V>::Reverse );
        errors += tester.validate();
        errors += tester.erase( count2, TreeTester<K,V>::Forward );
        errors += tester.validate();
        tester.logTree();
        tester.destroyTree();
        tester.createTree();
        errors += tester.insert( count1, TreeTester<K,V>::Reverse );
        errors += tester.validate();
        errors += tester.erase( count2, TreeTester<K,V>::Reverse );
        errors += tester.validate();
        tester.logTree();
        tester.destroyTree();
        tester.createTree();
        errors += tester.insert( count1, TreeTester<K,V>::Reverse );
        errors += tester.validate();
        errors += tester.erase( count2, TreeTester<K,V>::Random );
        errors += tester.validate();
        tester.logTree();
        tester.destroyTree();
        // Insertion in random order
        tester.createTree();
        errors += tester.validate();
        errors += tester.validate();
        errors += tester.insert( count1, TreeTester<K,V>::Random );
        errors += tester.validate();
        errors += tester.erase( count2, TreeTester<K,V>::Forward );
        errors += tester.validate();
        tester.logTree();
        tester.destroyTree();
        tester.createTree();
        errors += tester.insert( count1, TreeTester<K,V>::Random );
        errors += tester.validate();
        errors += tester.erase( count2, TreeTester<K,V>::Reverse );
        errors += tester.validate();
        tester.logTree();
        tester.destroyTree();
        tester.createTree();
        errors += tester.insert( count1, TreeTester<K,V>::Random );
        errors += tester.validate();
        errors += tester.erase( count2, TreeTester<K,V>::Random );
        errors += tester.validate();
        tester.logTree();
        tester.destroyTree();
        tester.destroyPool();
        // Endurance test by randomly inserting and removing ~33 % of B-Tree repeatedly
        log << "Endurance test...\n";
        tester.createPool();
        tester.createTree();
        errors += tester.insert( count1, TreeTester<K,V>::Random );
        errors += tester.validate();
        times( EnduranceCount ) {
            errors += tester.insert( count1/3, TreeTester<K,V>::Random );
            errors += tester.validate();
            errors += tester.erase( count1/3, TreeTester<K,V>::Random );
            errors += tester.validate();
        }
        tester.destroyTree();
        tester.destroyPool();
        // Commit and recover tests on persistent page pool
        log << "Persistent commit and recover test...\n";
        tester.persistent( true );
        tester.createPool();
        tester.createTree();
        errors += tester.validate();
        errors += tester.insert( count1/2, TreeTester<K,V>::Random );
        // tester.logTree();
        errors += tester.validate();
        times( TransactionCount ) {
            errors += tester.insert( count1/2, TreeTester<K,V>::Random );
            // tester.logTree();
            errors += tester.validate();
            errors += tester.commit();
            // tester.logTree();
            errors += tester.validate();
            errors += tester.replace( count1/2 );
            // tester.logTree();
            errors += tester.validate();
            errors += tester.recover();
            // tester.logTree();
            errors += tester.validate();
            errors += tester.erase( count1/2, TreeTester<K,V>::Random );
            // tester.logTree();
            errors += tester.validate();
            errors += tester.recover();
            // tester.logTree();
            errors += tester.validate();
            errors += tester.assign();
            // tester.logTree();
            errors += tester.validate();
            tester.destroyTree();
            tester.createTree();
            // tester.logTree();
            errors += tester.validate();
            tester.destroyTree();
            tester.destroyPool();
            tester.createPool();
            tester.createTree();
            // tester.logTree();
            errors += tester.validate();
            errors += tester.erase( count1/2, TreeTester<K,V>::Random );
            // tester.logTree();
            errors += tester.validate();
            errors += tester.commit();
            // tester.logTree();
            errors += tester.validate();
        }
        tester.destroyTree();
        tester.destroyPool();
        tester.deletePersistentStore();
    }
    catch ( string message ) {
        log << "Exception : " << message << "!\n";
        errors += 1;
        exception = true;
    }
    catch (...) {
        log << "Exception (...)!\n";
        errors += 1;
        exception = true;
    }
    log.flush();
    return { errors, exception };
}

int main(int argc, char* argv[]) {
    remove_all( "testBTreeValidity" );
    create_directory( "testBTreeValidity ");
    ofstream log;
    log.open( "testBTreeValidity\\logBTreeValidity.txt" );
    uint32_t errorCount = 0;
    uint32_t exceptionCount = 0;
    int count1 = 0;
    int count2 = 0;
    if (2 <= argc) count1 = stoi( argv[ 1 ] );
    if (3 <= argc) count2 = stoi( argv[ 2 ] );
    for ( int arg = 3; arg < argc; ++arg ) {
        pair<uint32_t,bool> errors;
        if (string( "Uint32Uint32" ) == argv[ arg ]) {
            log << "32-bit unsigned integer key to 32-bit unsigned integer B-Tree...\n" << flush;
            Uint32Uint32TreeTester tester( "testBTreeValidity", "Uint32Uint32", log );
            errors = doTest( tester, count1, count2, log );
        }
        if (string( "Uint16ArrayUint32" ) == argv[ arg ]) {
            log << "16-bit unsigned integer array key to 32-bit unsigned integer B-Tree.\n" << flush;
            Uint16ArrayUint32TreeTester tester( "testBTreeValidity", "Uint16ArrayUint32", log );
            errors = doTest( tester, count1, count2, log );
        }
        if (string( "Uint32Uint16Array" ) == argv[ arg ]) {
            log << "32-bit unsigned integer key to 16-bit unsigned integer array B-Tree.\n" << flush;
            Uint32Uint16ArrayTreeTester tester( "testBTreeValidity", "Uint32Uint16Array", log );
            errors = doTest( tester, count1, count2, log );
        }
        if (string( "Uint16ArrayUint16Array" ) == argv[ arg ]) {
            log << "16-bit unsigned integer array key to 16-bit unsigned integer array B-Tree.\n" << flush;
            Uint16ArrayUint16ArrayTreeTester tester( "testBTreeValidity", "Uint16ArrayUint16Array", log );
            errors = doTest( tester, count1, count2, log );
        }
        if (errors.second) {
            log << "Exception detected!\n";
            exceptionCount += 1;
        }
        if (0 < errors.first) {
            log << errors.first << " errors detected!\n";
            errorCount += errors.first;
        }
        log << "\n";
    }
    if (0 < exceptionCount) log << "Total of " << exceptionCount << " exceptions detected!\n";
    if (0 < errorCount) log << "Total of " << errorCount << " errors detected!\n";
    if ((exceptionCount ==0) && (errorCount ==0)) log << "No errors detected.\n";
    log.close();
    exit( errorCount );
};
