#include "BTree.h"
#include "PersistentPagePool.h"
#include <vector>
#include <set>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <random>
#include <algorithm>
#include <chrono>

using namespace BTree;
using namespace std;
using namespace std::filesystem;

// Test program to measure BTree performance.
// ToDo: Benchmark against std::map performance (both memory usage and speed) with default allocator

const int BTreePageSize = 4096;

const vector<int>  DefaultEntryCounts = { 1000, 10000, 100000, 1000000, 10000000 };
vector<int> EntryCounts;

const int MinArray = 2;
const int MaxArray = 15;

mt19937 gen32;
mt19937 gen64;

struct PageUsage {
    uint32_t    pages;
    uint32_t    freePages;
    uint32_t    pageCapacity;
    uint64_t    filling;
    uint64_t    payload;
};

template< class K, class V >
PageUsage pageUsage( const PagePool& pool ) {
    PageUsage usage;
    usage.pages = pool.size();
    usage.freePages = 0;
    usage.pageCapacity = pool.pageCapacity();
    usage.filling = 0;
    usage.payload = 0;
    for (uint32_t index = 0; index < usage.pages; ++index) {
        const PageHeader& page = pool.access( PageLink( index ) );
        if (page.free != 1) {
            if (page.depth == 0) {
                auto leaf = pool.page<B<K>,B<V>,A<K>,A<V>>( &page );
                usage.filling += leaf->filling();
                usage.payload += leaf->payload();
            } else {
                auto node = pool.page<B<K>,PageLink,A<K>,false>( &page );
                usage.filling += node->filling();
                usage.payload += node->payload();
            }
        } else {
            usage.freePages += 1;
        }
    }
    return usage;
}

template< class T >
T generateRandomValue() { return( gen64() % (uint64_t(1) << (sizeof( T ) * 8) ) ); }
uint32_t generateRandomLength( uint32_t min, uint32_t max ) {
    if (min < max) return (min + (gen32() % (max - min)));
    return min;
}

template< class T >
vector<T> generate( uint32_t min, uint32_t max ) {
    vector<T> value;
    uint32_t n = 0;
    while (n < min) { value.push_back( generateRandomValue<T>() ); ++n; }
    uint32_t N = generateRandomLength( min, max );
    while (n < N) { value.push_back( generateRandomValue<T>() ); ++n; }
    return value;
}

template< class T >
struct ArrayCompare {
    bool operator()( const vector<T> lhs, const vector<T> rhs ) const {
        if (compare( lhs.data(), PageSize(lhs.size()), rhs.data(), PageSize(rhs.size()) ) < 0) return( true );
        return( false );
    }
    static int compare( const T* lhs, PageSize ln, const T* rhs, PageSize rn ) {
        PageSize n = ((ln < rn) ? ln : rn);
        for (int i = 0; i < n; ++i) if (lhs[ i ] < rhs[ i ]) return( -1 ); else if (rhs[ i ] < lhs[ i ]) return( +1 );
        if (rn < ln) return( -1 );
        if (ln < rn) return( +1 );
        return( 0 );
    }
};

template< class T >
vector<vector<T>> generateUniqueKeys( uint32_t count, uint32_t min, uint32_t max ) {
    vector<vector<T>> keys;
    set<vector<T>,ArrayCompare<T>> keySet;
    for ( int i = 0; i < count; ++i) {
        auto key = generate<T>( min, max );
        while (keySet.count( key ) == 1) key = generate<T>( min, max );
        keys.push_back( key );
        keySet.insert( key );
    }
    return keys;
}

template< class T >
vector<vector<T>> generateValues( uint32_t count, uint32_t min, uint32_t max ) {
    vector<vector<T>> values;
    for ( int i = 0; i < count; ++i) values.push_back( generate<T>( min, max ) );
    return values;
}

template< class K, class V, std::enable_if_t<(!A<K>&&!A<V>),bool> = true >
uint32_t accessKeyValue(  const Tree<K,V>& tree, const vector<B<K>>& key, const vector<B<V>>& value ) {
    return( ( key[ 0 ] != value[ 0 ] ) ? 1 : 0 );
}
template< class K, class V, std::enable_if_t<(!A<K>&&A<V>),bool> = true >
uint32_t accessKeyValue(  const Tree<K,V>& tree, const vector<B<K>>& key, const vector<B<V>>& value ) {
    return( ( (key.size() != MinArray) || (key[ 0 ] != value[ 0 ]) ) ? 1 : 0 );
}
template< class K, class V, std::enable_if_t<(A<K>&&!A<V>),bool> = true >
uint32_t accessKeyValue(  const Tree<K,V>& tree, const vector<B<K>>& key, const vector<B<V>>& value ) {
    return( ( ( value.size() != MinArray) || (key[ 0 ] != value[ 0 ]) ) ? 1 : 0 );
}
template< class K, class V, std::enable_if_t<(A<K>&&A<V>),bool> = true >
uint32_t accessKeyValue(  const Tree<K,V>& tree, const vector<B<K>>& key, const vector<B<V>>& value ) {
    return( ( (key.size() != value.size()) || (key[ 0 ] != value[ 0 ]) ) ? 1 : 0 );
}
template< class K, class V, std::enable_if_t<(!A<K>&&!A<V>),bool> = true >
void treeInsert( Tree<K,V>& tree, const vector<B<K>>& key, const vector<B<V>>& value ) {
    tree.insert( key[ 0 ], value[ 0 ] );
}
template< class K, class V, std::enable_if_t<(A<K>&&!A<V>),bool> = true >
void treeInsert( Tree<K,V>& tree, const vector<B<K>>& key, const vector<B<V>>& value ) {
    tree.insert( key.data(), key.size(), value[ 0 ] );
}
template< class K, class V, std::enable_if_t<(!A<K>&&A<V>),bool> = true >
void treeInsert( Tree<K,V>& tree, const vector<B<K>>& key, const vector<B<V>>& value ) {
    tree.insert( key[ 0 ], value.data(), value.size() );
}
template< class K, class V, std::enable_if_t<(A<K>&&A<V>),bool> = true >
void treeInsert( Tree<K,V>& tree, const vector<B<K>>& key, const vector<B<V>>& value ) {
    tree.insert( key.data(), key.size(), value.data(), value.size() );
}
template< class K, class V, std::enable_if_t<(!A<K>&&!A<V>),bool> = true >
void treeReplace( Tree<K,V>& tree, const vector<B<K>>& key, const vector<B<V>>& value ) {
    tree.replace( key[ 0 ], value[ 0 ] );
}
template< class K, class V, std::enable_if_t<(A<K>&&!A<V>),bool> = true >
void treeReplace( Tree<K,V>& tree, const vector<B<K>>& key, const vector<B<V>>& value ) {
    tree.replace( key.data(), key.size(), value[ 0 ] );
}
template< class K, class V, std::enable_if_t<(!A<K>&&A<V>),bool> = true >
void treeReplace( Tree<K,V>& tree, const vector<B<K>>& key, const vector<B<V>>& value ) {
    tree.replace( key[ 0 ], value.data(), value.size() );
}
template< class K, class V, std::enable_if_t<(A<K>&&A<V>),bool> = true >
void treeReplace( Tree<K,V>& tree, const vector<B<K>>& key, const vector<B<V>>& value ) {
    tree.replace( key.data(), key.size(), value.data(), value.size() );
}
template< class K, class V, std::enable_if_t<(!A<K>),bool> = true >
void treeRetrieve( const Tree<K,V>& tree, const vector<B<K>>& key ) {
    auto result = tree.at( key[ 0 ] );
}
template< class K, class V, std::enable_if_t<(A<K>),bool> = true >
void treeRetrieve( const Tree<K,V>& tree, const vector<B<K>>& key ) {
    auto result = tree.at( key.data(), key.size() );
}
template< class K, class V, std::enable_if_t<(!A<K>),bool> = true >
void treeRemove( Tree<K,V>& tree, const vector<B<K>>& key ) {
    tree.erase( key[ 0 ] );
}
template< class K, class V, std::enable_if_t<(A<K>),bool> = true >
void treeRemove( Tree<K,V>& tree, const vector<B<K>>& key ) {
    tree.erase( key.data(), key.size() );
}

template< class K, class V >
class PerformanceTest {
    PagePool* pool;
    BTree::Tree<K,V>* tree;
    ofstream& log;
    pair<double,uint32_t> calibrate( uint32_t iterations, vector<vector<B<K>>>& keys, vector<vector<B<V>>>& values ) {
        uint32_t matches = 0;
        auto t0 = chrono::steady_clock::now();;
        for ( uint32_t i = 0; i < iterations; i++ ) matches += accessKeyValue( *tree, keys[ i ], values[ i ] );
        auto t1 = chrono::steady_clock::now();
        chrono::duration<double> elapsed = (t1 - t0);
        return { ((elapsed.count() / iterations) * 1000000), matches };
    };
    double insert( uint32_t iterations, vector<vector<B<K>>>& keys, vector<vector<B<V>>>& values ) {
        auto t0 = chrono::steady_clock::now();;
        for ( uint32_t i = 0; i < iterations; i++ ) treeInsert<K,V>( *tree, keys[ i ], values[ i ] );
        auto t1 = chrono::steady_clock::now();
        chrono::duration<double> elapsed = (t1 - t0);
        return ((elapsed.count() / iterations) * 1000000);
    };
    double replace( uint32_t iterations, vector<vector<B<K>>>& keys, vector<vector<B<V>>>& values ) {
        shuffle( keys.begin(), keys.end(), gen32 );
        auto t0 = chrono::steady_clock::now();;
        for ( uint32_t i = 0; i < iterations; i++ ) treeReplace<K,V>( *tree, keys[ i ], values[ i ] );
        auto t1 = chrono::steady_clock::now();
        chrono::duration<double> elapsed = (t1 - t0);
        return ((elapsed.count() / iterations) * 1000000);
    };
    double at( uint32_t iterations, vector<vector<B<K>>>& keys ) {
        shuffle( keys.begin(), keys.end(), gen32 );
        auto t0 = chrono::steady_clock::now();;
        for ( uint32_t i = 0; i < iterations; i++ ) treeRetrieve<K,V>( *tree, keys[ i ] );
        auto t1 = chrono::steady_clock::now();
        chrono::duration<double> elapsed = (t1 - t0);
        return ((elapsed.count() / iterations) * 1000000);
    };
    double erase( uint32_t iterations, vector<vector<B<K>>>& keys ) {
        shuffle( keys.begin(), keys.end(), gen32 );
        auto t0 = chrono::steady_clock::now();;
        for ( uint32_t i = 0; i < iterations; i++ ) treeRemove<K,V>( *tree, keys[ i ] );
        auto t1 = chrono::steady_clock::now();
        chrono::duration<double> elapsed = (t1 - t0);
        return ((elapsed.count() / iterations) * 1000000);
    };
    double commit() {
        auto t0 = chrono::steady_clock::now();;
        tree->commit();
        auto t1 = chrono::steady_clock::now();
        chrono::duration<double> elapsed = (t1 - t0);
        return elapsed.count();
    };
public:
    PerformanceTest() = delete;
    PerformanceTest( uint32_t pageSize, string name, ofstream& stream ) :
        pool( new PersistentPagePool( pageSize, string( "testBTreePerformance\\" ) + name + string( ".bt" ) ) ),
        tree( new BTree::Tree<K,V>( *pool ) ),
        log( stream )
    {
        tree->enableStatistics();
    };
    ~PerformanceTest() {
        delete tree;
        delete pool;
    }
    void logStatistics( bool clear ) const {
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
            if (clear) tree->clearStatistics();
        }
    }
    void measureUsage() const {
        PageUsage usage = pageUsage<K,V>( *pool );
        log << "Pages      : " << usage.pages << " [" << usage.pageCapacity << "]";
        if (0 < usage.freePages) log << " (" << usage.freePages << " free)";
        log << "\n";
        log.precision( 3 );
        log << "Filling    : " << ((100.0 * usage.filling) / (usage.pageCapacity * (usage.pages - usage.freePages))) << " %\n";
        log.precision( 3 );
        log << "Payload    : " << ((100.0 * usage.payload) / (usage.pageCapacity * (usage.pages - usage.freePages))) << " %\n";
    }
    void measurePerformance( uint32_t iterations ) {
        vector<vector<B<K>>> keys = generateUniqueKeys<B<K>>( iterations, (A<K> ? MinArray : 1), (A<K> ? MaxArray : 1) );
        vector<vector<B<V>>> values = generateValues<B<V>>( iterations, (A<V> ? MinArray : 1), (A<V> ? MaxArray : 1) );
        vector<vector<B<V>>> replaceValues = generateValues<B<V>>( iterations, (A<V> ? MinArray : 1), (A<V> ? MaxArray : 1) );
        auto calibration = calibrate( iterations, keys, values );
        double overhead = calibration.first;
        log.precision( 3 );
        log << "Calibration time " << overhead << " usec with " << iterations << " iterations";
        if (calibration.second < iterations) log << " (" << (iterations - calibration.second) << " misses)";
        log << ".\n";
        tree->clearStatistics();
        auto entryInsert = insert( iterations, keys, values );
        log.precision( 3 );
        log << "Random insert " << (entryInsert - overhead) << " usec.\n";
        measureUsage();
        logStatistics( true );
        auto elapsedCommit = commit();
        log.precision( 3 );
        log << "Commit " << elapsedCommit << " sec.\n";
        logStatistics( true );
        auto entryReplace = replace( iterations, keys, replaceValues );
        log.precision( 3 );
        log << "Random replace " << (entryReplace - overhead) << " usec.\n";
        measureUsage();
        logStatistics( true );
        auto entryRetrieve = at( iterations, keys );
        log.precision( 3 );
        log << "Random at " << (entryRetrieve - overhead) << " usec.\n";
        measureUsage();
        logStatistics( true );
        auto entryRemove = erase( iterations, keys );
        log.precision( 3 );
        log << "Random erase " << (entryRemove - overhead) << " usec.\n";
        measureUsage();
        logStatistics( true );
        log.flush();
    }
};

int main(int argc, char* argv[]) {
    remove_all( "testBTreePerformance" );
    create_directory( "testBTreePerformance ");
    int errors = 0;
    ofstream log;
    if ( 2 <= argc ) {
        for (int arg = 1; arg < argc; ++arg) { EntryCounts.push_back( stoi( argv[ arg ] ) ); }
    } else {
        EntryCounts = DefaultEntryCounts;
    }
    log.open( "testBTreePerformance\\logBTreePerformance.txt" );
    log << "32-bit integer key to 32-bit integer value performance ...\n";
    log.flush();
    try {
        for ( auto count : EntryCounts ) {
            stringstream bTreeName;
            bTreeName << "Uint32Uint32[ " << count << " ]";
            PerformanceTest<uint32_t,uint32_t> perform( BTreePageSize, bTreeName.str(), log );
            perform.measurePerformance( count );
        }
    }
    catch ( string message ) {
        log << message << "!\n";
        errors += 1;
    }
    catch (...) {
        log << "Exception!\n";
        errors += 1;
    }
    log << "\n32-bit integer key to 16-bit integer array[ " << MinArray << " - " << MaxArray << " ] value performance ...\n";
    log.flush();
    try {
        for ( auto count : EntryCounts ) {
            stringstream bTreeName;
            bTreeName << "Uint32Uint16Array[ " << count << " ]";
            PerformanceTest<uint32_t,uint16_t[]> perform( BTreePageSize, bTreeName.str(), log );
            perform.measurePerformance( count );
        }
    }
    catch ( string message ) {
        log << message << "!\n";
        errors += 1;
    }
    catch (...) {
        log << "Exception!\n";
        errors += 1;
    }
    log << "\n16-bit integer array[ " << MinArray << " - " << MaxArray << " ] key to 32-bit integer value performance ...\n";
    log.flush();
    try {
        for ( auto count : EntryCounts ) {
            stringstream bTreeName;
            bTreeName << "Uint16ArrayUint32[ " << count << " ]";
            PerformanceTest<uint16_t[],uint32_t> perform( BTreePageSize, bTreeName.str(), log );
            perform.measurePerformance( count );
        }
    }
    catch ( string message ) {
        log << message << "!\n";
        errors += 1;
    }
    catch (...) {
        log << "Exception!\n";
        errors += 1;
    }
    log << "\n16-bit integer array[ " << MinArray << " - " << MaxArray << " ] key to 16-bit integer array[ " << MinArray << " - " << MaxArray << " ] value performance ...\n";
    log.flush();
    try {
        for ( auto count : EntryCounts ) {
            stringstream bTreeName;
            bTreeName << "Uint16ArrayUint16Array[ " << count << " ]";
            PerformanceTest<uint16_t[],uint16_t[]> perform( BTreePageSize, bTreeName.str(), log );
            perform.measurePerformance( count );
        }
    }
    catch ( string message ) {
        log << message << "!\n";
        errors += 1;
    }
    catch (...) {
        log << "Exception!\n";
        errors += 1;
    }
    if (0 < errors) {
        log << "\n\n" << errors << " errors detected!";
    } else {
        log << "\n\nNo errors detected.";
    }
    log.flush();
    log.close();
    exit( errors );
};
