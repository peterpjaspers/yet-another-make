#include "Forest.h"
#include "PersistentPagePool.h"
#include <iostream>
#include <fstream>
#include <random>
#include <map>
#include <algorithm>

using namespace BTree;
using namespace std;

const int BTreePageSize = 512;
const int MinString = 2;
const int MaxString = 15;
const int MinArray = 2;
const int MaxArray = 15;

mt19937 gen32;

const int ValueCount = 100;

inline uint32_t generateUint32() {
    return (gen32() % 10000000);    
}
vector<uint16_t> generateUint16Array() {
    vector<uint16_t> value;
    int n = (MinArray  + (gen32() % (MaxArray - MinArray)));
    for (int i = 0; i < n; i++) value.push_back( gen32() % 10000 );
    return value;
}

PagePool* createPagePool( PageSize pageSize, bool persistent = false, string path = ""  ) {
    if ( persistent ) {
        // Determine stored page size (if any)...
        PageSize storedPageSize = PersistentPagePool::pageCapacity( path );
        return new PersistentPagePool( ((0 < storedPageSize) ? storedPageSize : pageSize), path );
    } else {
        return new PagePool( pageSize );
    }
}

Forest* forest;
Tree<uint32_t,uint32_t>* uint32uint32Tree;
Tree<uint32_t,uint16_t[]>* uint32uint16ArrayTree;
Tree<uint16_t[],uint32_t>* uint16Arrayuint32Tree;
Tree<uint16_t[],uint16_t[]>* uint16Arrayuint16ArrayTree;
TreeIndex uint32uint32Index;
TreeIndex uint32uint16ArrayIndex;
TreeIndex uint16Arrayuint32Index;
TreeIndex uint16Arrayuint16ArrayIndex;

void addEntry( Tree<uint32_t,uint32_t>* tree ) {
    uint32_t key = generateUint32();
    uint32_t value = generateUint32();
    while (tree->exists( key )) key = generateUint32();
    tree->insert( key, value );
}
void addEntry( Tree<uint32_t,uint16_t[]>* tree ) {
    uint32_t key = generateUint32();
    vector<uint16_t> value = generateUint16Array();
    while (tree->exists( key )) key = generateUint32();
    tree->insert( key, value.data(), value.size() );
}
void addEntry( Tree<uint16_t[],uint32_t>* tree ) {
    vector<uint16_t> key = generateUint16Array();
    uint32_t value = generateUint32();
    while (tree->exists( key.data(), key.size() )) key = generateUint16Array();
    tree->insert( key.data(), key.size(), value );
}
void addEntry( Tree<uint16_t[],uint16_t[]>* tree ) {
    vector<uint16_t> key = generateUint16Array();
    vector<uint16_t> value = generateUint16Array();
    while (tree->exists( key.data(), key.size() )) key = generateUint16Array();
    tree->insert( key.data(), key.size(), value.data(), value.size() );
}
void populateTrees( int count ) {
    // Populate B-Trees
    for (int i = 0; i < count; i++) addEntry( uint32uint32Tree );
    for (int i = 0; i < count; i++) addEntry( uint32uint16ArrayTree );
    for (int i = 0; i < count; i++) addEntry( uint16Arrayuint32Tree );
    for (int i = 0; i < count; i++) addEntry( uint16Arrayuint16ArrayTree );
}
void streamTrees( ofstream& log, string title ) {
    log
        << title << "...\n"
        << *forest
        << "Uint32 -> Uint32 B-Tree " << uint32uint32Index << " in forest...\n"
        << *uint32uint32Tree
        << "Uint32 -> [ Uint16 ] B-Tree " << uint32uint16ArrayIndex << " in forest...\n"
        << *uint32uint16ArrayTree
        << "[ Uint16 ] -> Uint32 B-Tree " << uint16Arrayuint32Index << " in forest...\n"
        << *uint16Arrayuint32Tree
        << "[ Uint16 ] -> [ Uint16 ] B-Tree " << uint16Arrayuint16ArrayIndex << " in forest...\n"
        << *uint16Arrayuint16ArrayTree;
}
int validateTrees( ofstream& log, int count ) {
    int errors = 0;
    if (uint32uint32Tree->size() != count) {
        log << "Tree<uint32_t,uint32_t> " << uint32uint32Index << " has incorrect size!\n";
        errors += 1;
    }
    if (uint32uint16ArrayTree->size() != count) {
        log << "Tree<uint32_t,uint16_t[]> " << uint32uint16ArrayIndex << " has incorrect size!\n";
        errors += 1;
    }
    if (uint16Arrayuint32Tree->size() != count) {
        log << "Tree<uint16_t[],uint32_t> " << uint16Arrayuint32Index << " has incorrect size!\n";
        errors += 1;
    }
    if (uint16Arrayuint16ArrayTree->size() != (count + ValueCount) ) {
        log << "Tree<uint16_t[],uint16_t[]> " << uint16Arrayuint16ArrayIndex << " has incorrect size!\n";
        errors += 1;
    }
    return errors;
}

int main(int argc, char* argv[]) {
    ofstream log;
    system( "RMDIR /S /Q testBTreeForest" );
    system( "MKDIR testBTreeForest" );
    log.open( "testBTreeForest\\logBTreeForest.txt" );
    int errorCount = 0;
    try {
        PagePool* pool = createPagePool( BTreePageSize, true, "testBTreeForest\\Forest.bt" );
        forest = new Forest( *pool );
        log << "Create initial forest...\n";
        auto tree1 = forest->plant<uint32_t,uint32_t>();
        uint32uint32Tree = tree1.first;
        uint32uint32Index = tree1.second;
        uint32uint16ArrayIndex = 37;
        uint32uint16ArrayTree = forest->plant<uint32_t,uint16_t[]>( uint32uint16ArrayIndex );
        auto tree3 = forest->plant<uint16_t[],uint32_t>();
        uint16Arrayuint32Tree = tree3.first;
        uint16Arrayuint32Index = tree3.second;
        PagePool temp = *createPagePool( BTreePageSize, false, "" );
        Tree<uint16_t[],uint16_t[]> u16Au16A( temp );
        for (int i = 0; i < ValueCount; i++) addEntry( &u16Au16A );
        uint16Arrayuint16ArrayIndex = 47;
        uint16Arrayuint16ArrayTree = forest->plant<uint16_t[],uint16_t[]>( uint16Arrayuint16ArrayIndex, u16Au16A );
        streamTrees( log, "Populated forest with empty trees" );
        log << "Commit initial forest...\n";
        forest->commit();
        errorCount += validateTrees( log, 0 );
        log << "Populate trees in forest with entries...\n";
        populateTrees( ValueCount );
        streamTrees( log, "Populated forest with populated trees" );
        errorCount += validateTrees( log, ValueCount );
        log << "Recover to initial forest...\n";
        forest->recover();
        // Validate that all B-Trees are int intital state...
        streamTrees( log, "Forest recovered to empty trees" );
        errorCount += validateTrees( log, 0 );
        log << "Populate trees in forest with entries...\n";
        populateTrees( ValueCount );
        streamTrees( log, "Populated forest with populated trees" );
        log << "Commit populated forest...\n";
        forest->commit();
        errorCount += validateTrees( log, ValueCount );
        log << "Further populate trees in forest with entries...\n";
        populateTrees( ValueCount );
        streamTrees( log, "Populated forest with further populated trees" );
        errorCount += validateTrees( log, (2 * ValueCount) );
        log << "Recover to populated forest...\n";
        forest->recover();
        streamTrees( log, "Populated forest with populated trees" );
        errorCount += validateTrees( log, ValueCount );
        log << "Destroy trees...\n";
        delete forest;
        log << "Build trees from persistent store...\n";
        forest = new Forest( *pool );
        uint32uint32Tree = forest->access<uint32_t,uint32_t>( uint32uint32Index );
        uint32uint16ArrayTree = forest->access<uint32_t,uint16_t[]>( uint32uint16ArrayIndex );
        uint16Arrayuint32Tree = forest->access<uint16_t[],uint32_t>( uint16Arrayuint32Index );
        uint16Arrayuint16ArrayTree = forest->access<uint16_t[],uint16_t[]>( uint16Arrayuint16ArrayIndex );
        streamTrees( log, "Forest recovered to populated trees from persistent store" );
        errorCount += validateTrees( log, ValueCount );
    }
    catch ( string message ) {
        log << message << "\n";
    }
    catch (...) {
        log << "Exception!\n";
    }
    if (0 < errorCount) log << errorCount << " errors detected!";
    log << "Done...\n";
    log.close();
    exit( errorCount );
};