#include "String2ValueTree.h"
#include "String2StringTree.h"
#include "PersistentPagePool.h"
#include <iostream>
#include <fstream>

using namespace BTree;
using namespace std;

// ToDo: Test program does not account for insertion of identical keys, will incorrectly report failure

const int BTreePageSize = 4096;
const int ValueCount = 1000;
const int MinKeyString = 2;
const int MaxKeyString = 15;

std::string generateStringKey() {
    static char characters[ 26 + 26 + 10 + 1 ] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    char key[ MaxKeyString + 1 ];
    int n = (MinKeyString  + (rand() % (MaxKeyString - MinKeyString)));
    for (int i = 0; i < n; i++) key[ i ] = characters[ (rand() % (26 + 26 + 10)) ];
    key[ n ] = 0;
    return( std::string( key ) );
}
inline uint32_t generateUint32Value() {
    return (((rand() * 7919) + rand()) % 10000000);    
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
    // String2Value test
    stream.open( "testString2ValueBTree.txt" );
    try {
        BTree::String2ValueTree<uint32_t,false> tree(
            *createPagePool( true, "String2ValueBTree.bin", BTreePageSize )
        );
        stream << "Writing " << ValueCount << " 32-bit unsigned integers...\n";
        srand( 13 );
        for ( int i = 0; i < ValueCount; i++ ) {
            uint32_t value = generateUint32Value();
            tree.insert( generateStringKey(), value );
        }
        stream << "Reading " << ValueCount << " 32-bit unsigned integers...\n";
        srand( 13 );
        for ( int i = 0; i < ValueCount; i++ ) {
            uint32_t reference = generateUint32Value();
            uint32_t value = tree.retrieve( generateStringKey() );
            if ( value != reference ) {
                stream << "[ " << i << " ] value mismatch : expected " << reference << ", retrieved " << value << ".\n";
            }
        }
        tree.commit();
        stream << tree;
    }
    catch ( string message ) {
        stream << message << "\n";
    }
    catch (...) {
        stream << "Exception!\n";
    }
    stream << "Done...\n";
    stream.close();
    // String2String test
    stream.open( "testString2StringBTree.txt" );
    try {
        BTree::String2StringTree tree(
            *createPagePool( true, "String2StringBTree.bin", BTreePageSize )
        );
        stream << "Writing " << ValueCount << " strings...\n";
        srand( 13 );
        for ( int i = 0; i < ValueCount; i++ ) {
            std::string value = generateStringKey();
            tree.insert( generateStringKey(), value );
        }
        stream << "Reading " << ValueCount << " strings...\n";
        srand( 13 );
        for ( int i = 0; i < ValueCount; i++ ) {
            std::string reference = generateStringKey();
            std::string value = tree.retrieve( generateStringKey() );
            if ( value != reference ) {
                stream << "[ " << i << " ] value mismatch : expected " << reference << ", retrieved " << value << ".\n";
            }
        }
        tree.commit();
        stream << tree;
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
