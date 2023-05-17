#include "BTree.h"
#include "PersistentPagePool.h"
#include <iostream>
#include <fstream>
#include <chrono>

using namespace BTree;
using namespace std;

// Test program to measure BTree performance.

const int BTreePageSize = 4096;
const int EntryCount = 10000000;
// const int EntryCount = 1000;
const int MinArray = 2;
const int MaxArray = 15;

inline uint16_t generateUint16() {
    return( rand() % 10000 );
}
inline uint32_t generateUint32() {
    return( (((uint32_t)rand() * (uint32_t)2097593) + (uint32_t)rand()) % (uint32_t)1000000000 );
}
PageIndex generateUint16Array( uint16_t* value ) {
    int n = (MinArray  + (rand() % (MaxArray - MinArray)));
    for (int i = 0; i < n; i++) value[ i ] = generateUint16();
    return n;
}

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
    ofstream stream;
    stream.open( "testBTreePerformance\\log.txt" );
    stream << "32-bit integer key to 16-bit integer value performance - " << EntryCount << " entries.\n";
    stream.flush();
    try {
        PagePool *pool = createPagePool( true, "testBTreePerformance\\Uint32Uint16.bt", BTreePageSize );
        BTree::Tree<uint32_t,uint16_t> tree( *pool );
        vector<uint32_t> keys;
        vector<uint16_t> values;
        for ( int i = 0; i < EntryCount; i++ ) {
            keys.push_back( generateUint32() );
            values.push_back( generateUint16() );
        }
        auto t0 = chrono::steady_clock::now();;
        for ( int i = 0; i < EntryCount; i++ ) tree.insert( keys[ i ], values[ i ] );
        auto t1 = chrono::steady_clock::now();
        tree.commit();
        auto t2 = chrono::steady_clock::now();
        for ( int i = 0; i < EntryCount; i++ ) tree.retrieve( keys[ i ] );
        auto t3 = chrono::steady_clock::now();
        chrono::duration<double> elapsedInsertion = (t1 - t0);
        chrono::duration<double> elapsedCommit = (t2 - t1);
        chrono::duration<double> elapsedRetrieval = (t3 - t2);
        double entryInsertion = ((elapsedInsertion.count() / EntryCount) * 1000000);
        double entryRetrieval = ((elapsedRetrieval.count() / EntryCount) * 1000000);
        stream.precision( 3 );
        stream << "Insertion " << entryInsertion << " usec, retrieval " << entryRetrieval << " usec\n";
        stream << "Commit " << elapsedCommit.count() << " sec [ " << pool->size() << " " << BTreePageSize << " byte pages ].\n";
    }
    catch ( string message ) {
        stream << message << "\n";
    }
    catch (...) {
        stream << "Exception!\n";
    }
    stream << "32-bit integer key to 16-bit integer array[ " << MinArray << " - " << MaxArray << " ] value performance - " << EntryCount << " entries.\n";
    stream.flush();
    try {
        PagePool *pool = createPagePool( true, "testBTreePerformance\\Uint32Uint16Array.bt", BTreePageSize );
        BTree::Tree<uint32_t,uint16_t,false,true> tree( *pool );
        vector<uint32_t> keys;
        vector<uint16_t*> values;
        vector<PageIndex> valueSizes;
        for ( int i = 0; i < EntryCount; i++ ) {
            keys.push_back( generateUint32() );
            uint16_t valueArray[ MaxArray ];
            PageIndex valueSize = generateUint16Array( valueArray );
            uint16_t *value = static_cast<uint16_t*>( malloc( valueSize * sizeof( uint16_t ) ) );
            memcpy( value, valueArray, (valueSize * sizeof( uint16_t )) );
            values.push_back( value );
            valueSizes.push_back( valueSize );
        }
        auto t0 = chrono::steady_clock::now();;
        for ( int i = 0; i < EntryCount; i++ ) tree.insert( keys[ i ], values[ i ], valueSizes[ i ] );
        auto t1 = chrono::steady_clock::now();
        tree.commit();
        auto t2 = chrono::steady_clock::now();
        for ( int i = 0; i < EntryCount; i++ ) tree.retrieve( keys[ i ] );
        auto t3 = chrono::steady_clock::now();
        chrono::duration<double> elapsedInsertion = (t1 - t0);
        chrono::duration<double> elapsedCommit = (t2 - t1);
        chrono::duration<double> elapsedRetrieval = (t3 - t2);
        double entryInsertion = ((elapsedInsertion.count() / EntryCount) * 1000000);
        double entryRetrieval = ((elapsedRetrieval.count() / EntryCount) * 1000000);
        stream.precision( 3 );
        stream << "Insertion " << entryInsertion << " usec, retrieval " << entryRetrieval << " usec\n";
        stream << "Commit " << elapsedCommit.count() << " sec [ " << pool->size() << " " << BTreePageSize << " byte pages ].\n";
    }
    catch ( string message ) {
        stream << message << "\n";
    }
    catch (...) {
        stream << "Exception!\n";
    }
    stream << "16-bit integer array[ " << MinArray << " - " << MaxArray << " ] key to 16-bit integer value performance - " << EntryCount << " entries.\n";
    stream.flush();
    try {
        PagePool *pool = createPagePool( true, "testBTreePerformance\\Uint16ArrayUint16.bt", BTreePageSize );
        BTree::Tree<uint16_t,uint16_t,true,false> tree( *pool );
        vector<uint16_t*> keys;
        vector<PageIndex> keySizes;
        vector<uint16_t> values;
        for ( int i = 0; i < EntryCount; i++ ) {
            uint16_t keyArray[ MaxArray ];
            PageIndex keySize = generateUint16Array( keyArray );
            uint16_t *key = static_cast<uint16_t*>( malloc( keySize * sizeof( uint16_t ) ) );
            memcpy( key, keyArray, (keySize * sizeof( uint16_t )) );
            keys.push_back( key );
            keySizes.push_back( keySize );
            values.push_back( generateUint16() );
        }
        auto t0 = chrono::steady_clock::now();;
        for ( int i = 0; i < EntryCount; i++ ) tree.insert( keys[ i ], keySizes[ i ], values[ i ] );
        auto t1 = chrono::steady_clock::now();
        tree.commit();
        auto t2 = chrono::steady_clock::now();
        for ( int i = 0; i < EntryCount; i++ ) tree.retrieve( keys[ i ] , keySizes[ i ]);
        auto t3 = chrono::steady_clock::now();
        chrono::duration<double> elapsedInsertion = (t1 - t0);
        chrono::duration<double> elapsedCommit = (t2 - t1);
        chrono::duration<double> elapsedRetrieval = (t3 - t2);
        double entryInsertion = ((elapsedInsertion.count() / EntryCount) * 1000000);
        double entryRetrieval = ((elapsedRetrieval.count() / EntryCount) * 1000000);
        stream.precision( 3 );
        stream << "Insertion " << entryInsertion << " usec, retrieval " << entryRetrieval << " usec\n";
        stream << "Commit " << elapsedCommit.count() << " sec [ " << pool->size() << " " << BTreePageSize << " byte pages ].\n";
    }
    catch ( string message ) {
        stream << message << "\n";
    }
    catch (...) {
        stream << "Exception!\n";
    }
    stream << "16-bit integer array[ " << MinArray << " - " << MaxArray << " ] key to 16-bit integer array[ " << MinArray << " - " << MaxArray << " ] value performance - " << EntryCount << " entries.\n";
    stream.flush();
    try {
        PagePool *pool = createPagePool( true, "testBTreePerformance\\Uint16ArrayUint16Array.bt", BTreePageSize );
        BTree::Tree<uint16_t,uint16_t,true,true> tree( *pool );
        vector<uint16_t*> keys;
        vector<PageIndex> keySizes;
        vector<uint16_t*> values;
        vector<PageIndex> valueSizes;
        for ( int i = 0; i < EntryCount; i++ ) {
            uint16_t keyArray[ MaxArray ];
            PageIndex keySize = generateUint16Array( keyArray );
            uint16_t *key = static_cast<uint16_t*>( malloc( keySize * sizeof( uint16_t ) ) );
            memcpy( key, keyArray, (keySize * sizeof( uint16_t )) );
            keys.push_back( key );
            keySizes.push_back( keySize );
            uint16_t valueArray[ MaxArray ];
            PageIndex valueSize = generateUint16Array( valueArray );
            uint16_t *value = static_cast<uint16_t*>( malloc( valueSize * sizeof( uint16_t ) ) );
            memcpy( value, valueArray, (valueSize * sizeof( uint16_t )) );
            values.push_back( value );
            valueSizes.push_back( valueSize );
        }
        auto t0 = chrono::steady_clock::now();;
        for ( int i = 0; i < EntryCount; i++ ) tree.insert( keys[ i ], keySizes[ i ], values[ i ], valueSizes[ i ] );
        auto t1 = chrono::steady_clock::now();
        tree.commit();
        auto t2 = chrono::steady_clock::now();
        for ( int i = 0; i < EntryCount; i++ ) tree.retrieve( keys[ i ] , keySizes[ i ]);
        auto t3 = chrono::steady_clock::now();
        chrono::duration<double> elapsedInsertion = (t1 - t0);
        chrono::duration<double> elapsedCommit = (t2 - t1);
        chrono::duration<double> elapsedRetrieval = (t3 - t2);
        double entryInsertion = ((elapsedInsertion.count() / EntryCount) * 1000000);
        double entryRetrieval = ((elapsedRetrieval.count() / EntryCount) * 1000000);
        stream.precision( 3 );
        stream << "Insertion " << entryInsertion << " usec, retrieval " << entryRetrieval << " usec\n";
        stream << "Commit " << elapsedCommit.count() << " sec [ " << pool->size() << " " << BTreePageSize << " byte pages ].\n";
    }
    catch ( string message ) {
        stream << message << "\n";
    }
    catch (...) {
        stream << "Exception!\n";
    }
    stream.flush();
    stream.close();
};
