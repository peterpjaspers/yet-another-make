#include "PersistentPagePool.h"
#include "BTree.h"
#include "Forest.h"
#include "StreamingBTree.h"
#include "PageStreamASCII.h"
#include <vector>
#include <set>
#include <iostream>
#include <fstream>
#include <filesystem>

using namespace BTree;
using namespace std;
using namespace std::filesystem;

#define each(i,count) for(int i = 0; i < int(count); ++i)
#define times(count) for(int i = 0; i < int(count); ++i)

// Test program B-Tree file validity.
// Number of attempts to detect unexpected B-Tree content
const int ProbeCount = 100;
const int EnduranceCount = 10;
const int TransactionCount = 10;

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

uint32_t validatePersistentPagePool( ostream& log, PageSize pageSize, path persistentFile ) {
    uint32_t errors = 0;
    log << "Reading from persistent page file " << persistentFile << "\n";
    fstream file( persistentFile, ios::in | ios::binary );
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
                        (page->capacity != pageSize)
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
                        (page->capacity != pageSize)
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
    return errors;
}

void logPageList( ostream& log, const vector<PageLink>& list, const string tag ) {
    log << tag << " pages [";
    if (0 < list.size()) {
        log << " " << list[ 0 ];
        for ( int i = 1; i < list.size(); ++i ) log << ", " << list[ i ];
    }
    log << " ]\n";
}

template< class KT, class VT, bool KA, bool VA >
pair<uint32_t,uint32_t> validateNode( ostream& log, PagePool& pool, set<PageLink>& pageLinks, const Page<KT,PageLink,KA,false>& node, PageDepth depth ) {
    uint32_t errors = 0;
    uint32_t pageCount = 0;
    if (node.splitDefined()) {
        auto result = validatePage<KT,VT,KA,VA>( log, pool, pageLinks, node.split(), (depth - 1) );
        errors += result.first;
        pageCount += result.second;
    }
    for (PageIndex i = 0; i < node.size(); ++i) {
        auto result = validatePage<KT,VT,KA,VA>( log, pool, pageLinks, node.value( i ), (depth - 1) );
        errors += result.first;
        pageCount += result.second;
    }
    return{ errors, pageCount };

}
template< class KT, class VT, bool KA, bool VA >
pair<uint32_t,uint32_t> validatePage( ostream& log, PagePool& pool, set<PageLink>& pageLinks, PageLink link, PageDepth depth ) {
    uint32_t errors = 0;
    if (link.null()) {
        log << "Accessing null link!\n";
        errors += 1;
        return{ errors, 0 };
    }
    if ( pool.size() <= link.index ) {
        log << "Invalid PageLink index " << link.index << " exceeds pool size " << pool.size() << "!\n" ;
        errors += 1;
        return{ errors, 0 };
    }
    auto result = pageLinks.insert( link );
    if (!result.second) {
        log << "Malformed B-Tree (cycles or merged branches) at " << link << "!\n";
        errors += 1;
        return{ errors, 0 };
    }
    const PageHeader& page = pool.access( link );
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
        auto node( pool.page<KT,PageLink,KA,false>( &page ) );
        log << *node;
        auto result = validateNode<KT,VT,KA,VA>( log, pool, pageLinks, *node, depth );
        errors += result.first;
        pageCount += result.second;
    } else {
        auto leaf( pool.page<KT,VT,KA,VA>( &page ) );
        log << *leaf;
        
    }
    return{ errors, pageCount };
}
template< class KT, class VT >
uint32_t validatePagePool( ostream& log, PagePool& pool, PageLink root ) {
    uint32_t errors = 0;
    uint32_t pageCount = 0;
    // Set of PageLinks of pages belonging to B-Tree.
    // Used to detect malformed B-Tree, pure hierarchy expected.
    // Detects e.g. cyclic dependencies and nodes with multiple parents
    set<PageLink> pageLinks;
    // Validate all pages belonging to the B-Tree (recursive from root)
    auto result( validatePage<B<KT>,B<VT>,A<KT>,A<VT>>( log, pool, pageLinks, root, UINT16_MAX ) );
    errors = result.first;
    pageCount = result.second;
    // Determine average page filling (informational)...
    uint64_t totalUsage = 0;
    for (auto link : pageLinks) {
        const PageHeader& page = pool.access( link );
        if (page.depth == 0) {
            auto leaf = pool.page<B<KT>,B<VT>,A<KT>,A<VT>>( &page );
            totalUsage += leaf->filling();
        } else {
            auto node = pool.page<B<KT>,PageLink,A<KT>,false>( &page );
            totalUsage += node->filling();
        }
    }
    uint64_t capacity = (pageCount * pool.pageCapacity());
    log << "B-Tree size " << totalUsage
        << " bytes, capacity " << capacity
        << " bytes, in " << pageCount
        << " pages, filling " << ((totalUsage * 100)/ capacity) << " %\n";
    // Now check to see if there are orphan pages.
    // An orphan is a non-free page that is not counted as a B-Tree page
    vector<PageLink> freePages;
    vector<PageLink> modifiedPages;
    vector<PageLink> recoverPages;
    vector<PageLink> persistentPages;
    for (uint32_t i = 0; i < pool.size(); ++i ) {
        const PageLink link( i );
        const PageHeader& page = pool.access( link );
        if (page.free != 0) freePages.push_back( link );
        if (page.modified != 0) modifiedPages.push_back( link );
        if (page.recover != 0) recoverPages.push_back( link );
        if (page.persistent != 0) persistentPages.push_back( link );
        if ((page.recover != 0) && (page.persistent == 0)) {
            log << "Recovering non-persistent page " << page.page << "!\n";
            errors += 1;
        }
    }
    if (freePages.size() != pool.sizeFreed()) {
        log << "Free pages list size " << pool.sizeFreed() << " does not match detected number of free pages " << freePages.size() << "!\n";
        errors += 1;
    }
    if (modifiedPages.size() != pool.sizeModified()) {
        log << "Modified pages list size " << pool.sizeModified() << " does not match detected number of modified pages " << modifiedPages.size() << "!\n";
        errors += 1;
    }
    if (recoverPages.size() != pool.sizeRecover()) {
        log << "Recover pages list size " << pool.sizeRecover() << " does not match detected number of recover pages " << recoverPages.size() << "!\n";
        errors += 1;
    }
    if (persistentPages.size() < recoverPages.size()) {
        log << "Number of recover pages " << recoverPages.size() << " exceeds number of persistent pages " << persistentPages.size() << "!/n";
    }
    if ((pageCount + freePages.size()) < pool.size()) {
        auto orphans = (pool.size() - (pageCount + freePages.size()));
        log << "Detected " << orphans << " orphans out of " << pool.size() << " pages, B-Tree used " << pageCount << " pages!\n";
        errors += 1;
    }
    log << "Page pool consists of " << pool.size() << " pages, "
        << freePages.size() << " free, "
        << modifiedPages.size() << " modified, "
        << persistentPages.size() << " persistent, "
        << recoverPages.size() << " recover.\n";
    logPageList( log, freePages, "Free" );
    logPageList( log, modifiedPages, "Modified" );
    logPageList( log, persistentPages, "Persistent" );
    logPageList( log, recoverPages, "Recover" );
    log << flush;
    return errors;
}

template< class K, class V>
uint32_t validatePagePoolFile( ostream& log, string persistentFile ) {
    uint32_t errors = 0;
    log << "Reading B-Tree page size...\n" << flush;
    auto pageSize( PersistentPagePool::pageCapacity( persistentFile ) );
    log << "Validating page pool file...\n" << flush;
    errors += validatePersistentPagePool( log, pageSize, persistentFile );
    if ( errors == 0 ) {
        PersistentPagePool pool( pageSize, persistentFile );
        log << "Validating page pool...\n" << flush;
        errors += validatePagePool<K,V>( log, pool, pool.commitRoot()->page );
    }
    return errors;
}

template<class K>
void validateStreamingTree( ostream& log, const Tree<StreamKey<K>,uint8_t[]>& tree ) {
    validatePagePool<StreamKey<K>,uint8_t[]>( log, tree.pagePool(), tree.rootLink() );
}

// Paramters are:
//
// 1 b-Tree file name
// 2 Log file name (optional)
//
// If a log file is defined, results are written to the log file otherwise resuts are written to standard out.
//
int main(int argc, char* argv[]) {
    ostream* log = &cout;
    ofstream fileLog;
    if ((3 < argc) || (argc < 1)) *log << "Invalid program arguments!" << endl;
    if (argc == 3) {
        fileLog.open( argv[2] );
        log = &fileLog;
    }
    string bTreeFileName( argv[ 1 ] );
    *log << "Validating Forest..." << endl << endl;
    validatePagePoolFile<TreeIndex,PageLink>( *log, bTreeFileName );
    *log << endl << "Validating Trees in Forest..." << endl;
    auto capacity( PersistentPagePool::pageCapacity( bTreeFileName ) );
    PersistentPagePool pool( PersistentPagePool::pageCapacity( bTreeFileName ), bTreeFileName );
    Forest forest( pool );
    // for ( auto index : forest ) {
    //     StreamingTree<uint64_t>* tree( forest.accessStreamingTree<uint64_t>( index.first ) );
    //     *log << endl << "Validating StreamingTree<uint64_t> " << index.first << "..." << endl;
    //     validateStreamingTree<uint64_t>( *log, *tree );
    // }
    auto tree( forest.accessStreamingTree<uint64_t>( 12 ) );
    tree->erase( 0xc00000000000296 );
    tree->erase( 0xc0000000000000f );
    tree->erase( 0xc00000000000010 );
    tree->erase( 0xc00000000000011 );
    tree->erase( 0x400000000000012 );
    tree->erase( 0xc00000000000016 );
    tree->erase( 0xc00000000000015 );
    tree->erase( 0xc00000000000053 );
    tree->erase( 0xc00000000000054 );
    tree->erase( 0xc00000000000051 );
    tree->erase( 0xc00000000000050 );
    tree->erase( 0xc00000000000055 );
    tree->erase( 0xc0000000000004d );
    tree->erase( 0xc0000000000004e );
    tree->erase( 0xc0000000000004c );
    tree->erase( 0x50000000000004b );
    tree->erase( 0xc00000000000090 );
    tree->erase( 0xc000000000002dd );
    tree->erase( 0xc00000000000091 );
    tree->erase( 0xc00000000000285 );
    tree->erase( 0xc000000000002cf );
    tree->erase( 0xc000000000002d0 );
    tree->erase( 0xc0000000000008c );
    tree->erase( 0xc0000000000008f );
    tree->erase( 0x50000000000008d );
    tree->erase( 0xc0000000000008e );
    tree->erase( 0xc000000000000b8 );
    tree->erase( 0xc000000000000b7 );
    tree->erase( 0xc000000000000d4 );
    tree->erase( 0xc000000000000d6 );
    tree->erase( 0xc0000000000032b );
    tree->erase( 0xc00000000000291 );
    tree->erase( 0x500000000000290 );
    tree->erase( 0xc000000000000b1 );
    tree->erase( 0xc000000000000ca );
    tree->erase( 0xc000000000000cb );
    tree->erase( 0xc000000000000cc );
    tree->erase( 0xc0000000000011b );
    tree->erase( 0xc0000000000011c );
    tree->erase( 0xc0000000000011a );
    tree->erase( 0x4000000000000db );
    tree->erase( 0xc00000000000031 );
    tree->erase( 0xc000000000000d1 );
    tree->erase( 0xc000000000000d2 );
    tree->erase( 0xc000000000000d3 );
    tree->erase( 0xc000000000000d0 );
    tree->erase( 0xc000000000000cf );
    tree->erase( 0xc000000000000ce );
    tree->erase( 0xc00000000000041 );
    tree->erase( 0xc00000000000040 );
    tree->erase( 0xc00000000000042 );
    validateStreamingTree<uint64_t>( *log, *tree );
    tree->erase( 0xc00000000000089 );
};

// Type specfiers are defined through the syntax:
//
// typeSpeficier := key '->' value
// key := type
// value := type
// type := scalar | scalar '[]'
// scalar := ( 'S' | 'U' ) ( '8' | '16' | '32' | '64' )
//
// For example:
//
//  U16[] -> S64
//  16-bit unsigned integer array keys, Signed 64-bit integer values
//
//  U64 -> U8[]
//  Unsigned 64-bit keys, Byte array values
//
//    string typeSpecifier( argv[ 2 ] );
//    auto type( parseType( typeSpecifier ) );
// string parseType( const string& typeSpecifier ) {
//     const char* typeStrings[ 16 ] = {
//         "U8", "U16", "U32", "U64",
//         "S8", "S16", "S32", "S64",
//         "U8[]", "U16[]", "U32[]", "U64[]",
//         "S8[]", "S16[]", "S32[]", "S64[]",
//     };
//     string key( "" ), value( "" );
//     each( i, 16 ) {
//         auto found( typeSpecifier.find( typeStrings[ i ] ) );
//         if ( found != typeSpecifier.npos ) { key = typeStrings[ i ]; break; }
//     }
//     if ( typeSpecifier.find( "->" ) != typeSpecifier.npos ) {
//         each( i, 16 ) {
//             auto found( typeSpecifier.find( typeStrings[ i ] ) );
//             if ( found != typeSpecifier.npos ) { value = typeStrings[ i ]; break; }
//         }
//     }
//     return key + "->" + value;
// }

