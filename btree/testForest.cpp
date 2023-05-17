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

inline uint32_t generateUint32() {
    return (gen32() % 10000000);    
}
vector<uint16_t> generateUint16Array() {
    vector<uint16_t> value;
    int n = (MinArray  + (gen32() % (MaxArray - MinArray)));
    for (int i = 0; i < n; i++) value.push_back( gen32() % 10000 );
    return value;
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

Forest* forest;
Tree<uint32_t,uint32_t>* uint32uint32Tree;
Tree<uint32_t,uint16_t[]>* uint32uint16ArrayTree;
Tree<uint16_t[],uint32_t>* uint16Arrayuint32Tree;
Tree<uint16_t[],uint16_t[]>* uint16Arrayuint16ArrayTree;
TreeIndex uint32uint32Index;
TreeIndex uint32uint16ArrayIndex;
TreeIndex uint16Arrayuint32Index;
TreeIndex uint16Arrayuint16ArrayIndex;

void populateTrees( int count ) {
    // Populate B-Trees
    for (int i = 0; i < count; i++) {
        uint32_t key = generateUint32();
        uint32_t value = generateUint32();
        uint32uint32Tree->insert( key, value );
    }
    for (int i = 0; i < count; i++) {
        uint32_t key = generateUint32();
        vector<uint16_t> value = generateUint16Array();
        uint32uint16ArrayTree->insert( key, value.data(), value.size() );
    }
    for (int i = 0; i < count; i++) {
        vector<uint16_t> key = generateUint16Array();
        uint32_t value = generateUint32();
        uint16Arrayuint32Tree->insert( key.data(), key.size(), value );
    }
    for (int i = 0; i < count; i++) {
        vector<uint16_t> key = generateUint16Array();
        vector<uint16_t> value = generateUint16Array();
        uint16Arrayuint16ArrayTree->insert( key.data(), key.size(), value.data(), value.size() );
    }
}
void streamTrees( ofstream& stream, string title ) {
    stream << title << "...\n";
    stream << *forest;
    stream << "Uint32 -> Uint32 B-Tree " << uint32uint32Index << " in forest...\n";
    stream << *uint32uint32Tree;
    stream << "Uint32 -> [ Uint16 ] B-Tree " << uint32uint16ArrayIndex << " in forest...\n";
    stream << *uint32uint16ArrayTree;
    stream << "[ Uint16 ] -> Uint32 B-Tree " << uint16Arrayuint32Index << " in forest...\n";
    stream << *uint16Arrayuint32Tree;
    stream << "[ Uint16 ] -> [ Uint16 ] B-Tree " << uint16Arrayuint16ArrayIndex << " in forest...\n";
    stream << *uint16Arrayuint16ArrayTree;
}

int main(int argc, char* argv[]) {
    ofstream stream;
    system( "RMDIR /S /Q testBTreeForest" );
    system( "MKDIR testBTreeForest" );
    stream.open( "testBTreeForest\\log.txt" );
    try {
        PagePool* pool = createPagePool( true, "testBTreeForest\\Forest.bt", BTreePageSize );
        forest = new Forest( *pool );
        stream << "Create empty forest...\n";
        auto tree1 = forest->plant<uint32_t,uint32_t>();
        uint32uint32Tree = tree1.first;
        uint32uint32Index = tree1.second;
        auto tree2 = forest->plant<uint32_t,uint16_t[]>();
        uint32uint16ArrayTree = tree2.first;
        uint32uint16ArrayIndex = tree2.second;
        auto tree3 = forest->plant<uint16_t[],uint32_t>();
        uint16Arrayuint32Tree = tree3.first;
        uint16Arrayuint32Index = tree3.second;
        auto tree4 = forest->plant<uint16_t[],uint16_t[]>();
        uint16Arrayuint16ArrayTree = tree4.first;
        uint16Arrayuint16ArrayIndex = tree4.second;
        streamTrees( stream, "Populated forest with empty trees" );
        stream << "Commit empty forest...\n";
        forest->commit();
        stream << "Populate trees in forest with 100 entries...\n";
        populateTrees( 100 );
        streamTrees( stream, "Populated forest with populated trees" );
        stream << "Recover to empty forest...\n";
        forest->recover();
        // Validate that all B-Trees are empty...
        for ( auto index : forest->begin() ) {
            // ToDo:
            // if (0 < forest->access( index )->size()) stream << "Tree " << index.first << " not empty!\n";
        }
        streamTrees( stream, "Forest recovered to empty trees" );
        stream << "Populate trees in forest with 100 entries...\n";
        populateTrees( 100 );
        streamTrees( stream, "Populated forest with populated trees" );
        stream << "Commit populated forest...\n";
        forest->commit();
        stream << "Further populate trees in forest with 100 entries...\n";
        populateTrees( 100 );
        streamTrees( stream, "Populated forest with further populated trees" );
        stream << "Recover to populated forest...\n";
        forest->recover();
        streamTrees( stream, "Populated forest with populated trees" );
        stream << "Destroy trees...\n";
        delete forest;
        stream << "Build trees from persistent store...\n";
        forest = new Forest( *pool );
        uint32uint32Tree = forest->access<uint32_t,uint32_t>( uint32uint32Index );
        uint32uint16ArrayTree = forest->access<uint32_t,uint16_t[]>( uint32uint16ArrayIndex );
        uint16Arrayuint32Tree = forest->access<uint16_t[],uint32_t>( uint16Arrayuint32Index );
        uint16Arrayuint16ArrayTree = forest->access<uint16_t[],uint16_t[]>( uint16Arrayuint16ArrayIndex );
        streamTrees( stream, "Forest recovered to populated trees from persistent store" );
    }
    catch ( string message ) {
        stream << message << "\n";
    }
    catch (...) {
        stream << "Exception!\n";
    }
    stream << "Done...\n";
    stream.close();
};
