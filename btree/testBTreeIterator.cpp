#include "BTree.h"
#include "BTreeIterator.h"
#include "PersistentPagePool.h"
#include <iostream>
#include <fstream>
#include <random>

using namespace BTree;
using namespace std;

const int BTreePageSize = 128;
const int ValueCount = 1000;
const int MinString = 2;
const int MaxString = 12;
const int MinArray = 2;
const int MaxArray = 12;

mt19937 gen32;

string generateString() {
    static char characters[ 26 + 26 + 10 + 1 ] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    char key[ MaxString + 1 ];
    int n = (MinString  + (gen32() % (MaxString - MinString)));
    for (int i = 0; i < n; i++) key[ i ] = characters[ (gen32() % (26 + 26 + 10)) ];
    key[ n ] = 0;
    return( string( key ) );
}
inline uint32_t generateUint32() {
    return (gen32() % 10000000);    
}
PageIndex generateUint16Array( uint16_t* value ) {
    int n = (MinArray  + (gen32() % (MaxArray - MinArray)));
    for (int i = 0; i < n; i++) value[ i ] = (gen32() % 10000);
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

// ToDo: Test key() and value() functions on iterator...

int main(int argc, char* argv[]) {
    system( "RMDIR /S /Q testBTreeIterator" );
    system( "MKDIR testBTreeIterator" );
    ofstream stream;
    // Value2String test - scalar keys
    stream.open( "testBTreeIterator\\logBTreeIterator.txt" );
    try {
        PagePool* pagePool = createPagePool( true, "testBTreeIterator\\logUint32Uint32.bt", BTreePageSize );
        BTree::Tree<uint32_t,uint32_t> tree( *pagePool );
        stream << "\n\nGenerating B-Tree<uint32_t,uint32_t> with " << ValueCount << " entries...\n";
        gen32.seed( 13 );
        uint32_t findKey;
        for ( int i = 0; i < ValueCount; i++ ) {
            uint32_t key = generateUint32();
            uint32_t value = generateUint32();
            if (i == (ValueCount / 3)) findKey = key;
            tree.insert( key, value );
        }
        tree.commit();
        stream << tree;
        {
            stream << "\nIterating forward from begin...\n";
            auto iterator = tree.begin();
            for (int i = 0; i < 20; i++) stream << "Value at begin [ " << i << " ] : " << (iterator++).key() << " -> " << iterator.value() << "\n" << flush;
        }
        {
            stream << "\nIterating in reverse from end...\n";
            auto iterator = tree.end();
            for (int i = 0; i < 20; i++) stream << "Value at end [ " << (-1 - i) << " ] = " << (--iterator).key() << " -> " << iterator.value() << "\n" << flush;
        }
        {
            stream << "\nIterating forward from " << findKey << " ...\n";
            auto iterator = tree.at( findKey );
            for (int i = 0; i < 20; i++) {
                auto entry = *iterator;
                stream << "Value at " << findKey << " [ " << i << " ] = " << entry.first << " -> " << entry.second << "\n" << flush;
                ++iterator;
            }
        }
        {
            stream << "\nIterating from begin to end...\n";
            size_t count = 0;
            for (auto iterator : tree) count += 1;
            stream << "B-Tree contains " << count << " entries\n";
        }
        stream << tree;
    }
    catch ( string message ) {
        stream << message << "\n";
    }
    catch (...) {
        stream << "Exception!\n";
    }
    try {
        PagePool* pagePool = createPagePool( true, "testBTreeIterator\\logCharArrayUint32.bt", BTreePageSize );
        BTree::Tree<char[],uint32_t> tree( *pagePool );
        stream << "\n\nGenerating B-Tree<char[],uint32_t> with " << ValueCount << " entries...\n";
        gen32.seed( 13 );
        string findKey;
        for ( int i = 0; i < ValueCount; i++ ) {
            string key = generateString();
            uint32_t value = generateUint32();
            if (i == (ValueCount / 3)) findKey = key;
            tree.insert( key.c_str(), key.size(), value );
        }
        tree.commit();
        stream << tree;
        {
            stream << "\nIterating forward from begin...\n";
            auto iterator = tree.begin();
            for (int i = 0; i < 20; i++) {
                auto key = iterator.key();
                auto value = iterator++.value();
                stream << "Value at begin [ " << i << " ] = " << string( key.first, key.second ) << " -> " << value << "\n" << flush;
            }
        }
        {
            stream << "\nIterating in reverse from end...\n";
            auto iterator = tree.end();
            for (int i = 0; i < 20; i++) {
                auto key = (--iterator).key();
                auto value = iterator.value();
                stream << "Value at end [ " << (-1 - i) << " ] = " << string( key.first, key.second ) << " -> " << value << "\n" << flush;
            }
        }
        {
            stream << "\nIterating forward from " << findKey << " ...\n";
            auto iterator = tree.at( findKey.c_str(), findKey.size() );
            for (int i = 0; i < 20; i++) {
                auto entry = *iterator++;
                stream << "Value at " << findKey << " [ " << i << " ] = " << string( entry.first.first, entry.first.second ) << " -> " << entry.second << "\n" << flush;
            }
        }
        {
            stream << "\nIterating from begin to end...\n";
            size_t count = 0;
            for (auto iterator : tree) count += 1;
            stream << "B-Tree contains " << count << " entries\n";
        }
    }
    catch ( string message ) {
        stream << message << "\n";
    }
    catch (...) {
        stream << "Exception!\n";
    }
    try {
        PagePool* pagePool = createPagePool( true, "testBTreeIterator\\logUint32CharArray.bt", BTreePageSize );
        BTree::Tree<uint32_t,char[]> tree( *pagePool );
        vector<string> values( ValueCount );
        stream << "\n\nGenerating B-Tree<uint32_t,char[]> with " << ValueCount << " entries...\n";
        gen32.seed( 13 );
        uint32_t findKey;
        for ( int i = 0; i < ValueCount; i++ ) {
            uint32_t key = generateUint32();
            values[ i ] = generateString();
            if (i == (ValueCount / 3)) findKey = key;
            tree.insert( key, values[ i ].c_str(), values[ i ].size() );
        }
        tree.commit();
        stream << tree;
        {
            stream << "\nIterating forward from begin...\n";
            auto iterator = tree.begin();
            for (int i = 0; i < 20; i++) {
                auto key = iterator.key();
                auto value = iterator++.value();
                stream << "Value at begin [ " << i << " ] = " << key << " -> " << string( value.first, value.second ) << "\n" << flush;
            }
        }
        {
            stream << "\nIterating in reverse from end...\n";
            auto iterator = tree.end();
            for (int i = 0; i < 20; i++) {
                auto key = (--iterator).key();
                auto value = iterator.value();
                stream << "Value at end [ " << (-1 - i) << " ] = " << key << " -> " << string( value.first, value.second ) << "\n" << flush;
            }
        }
        {
            stream << "\nIterating forward from " << findKey << " ...\n";
            auto iterator = tree.at( findKey );
            for (int i = 0; i < 20; i++) {
                auto entry = *iterator++;
                stream << "Value at " << findKey << " [ " << i << " ] = " << entry.first << " -> " << string( entry.second.first, entry.second.second ) << "\n" << flush;
            }
        }
        {
            stream << "\nIterating from begin to end...\n";
            size_t count = 0;
            for (auto iterator : tree) count += 1;
            stream << "B-Tree contains " << count << " entries\n";
        }
    }
    catch ( string message ) {
        stream << message << "\n";
    }
    catch (...) {
        stream << "Exception!\n";
    }
    try {
        PagePool* pagePool = createPagePool( true, "testBTreeIterator\\logCharArrayCharArray.bt", BTreePageSize );
        BTree::Tree<char[],char[]> tree( *pagePool );
        stream << "\n\nGenerating B-Tree<char[],char[]> with " << ValueCount << " entries...\n";
        vector<string> keys( ValueCount );
        vector<string> values( ValueCount );
        gen32.seed( 13 );
        string findKey;
        for ( int i = 0; i < ValueCount; i++ ) {
            keys[ i ] = generateString();
            values[ i ] = generateString();
            if (i == (ValueCount / 3)) findKey = keys[ i ];
            tree.insert( keys[ i ].c_str(), keys[ i ].size(), values[ i ].c_str(), values[ i ].size() );
        }
        tree.commit();
        stream << tree;
        {
            stream << "\nIterating forward from begin...\n";
            auto iterator = tree.begin();
            for (int i = 0; i < 20; i++) {
                auto key = iterator.key();
                auto value = iterator++.value();
                stream << "Value at begin [ " << i << " ] = " << string( key.first, key.second ) << " -> " << string( value.first, value.second ) << "\n" << flush;
            }
        }
        {
            stream << "\nIterating in reverse from end...\n";
            auto iterator = tree.end();
            for (int i = 0; i < 20; i++) {
                auto key = (--iterator).key();
                auto value = iterator.value();
                stream << "Value at end [ " << (-1 - i) << " ] = " << string( key.first, key.second ) << " -> " << string( value.first, value.second ) << "\n" << flush;
            }
        }
        {
            stream << "\nIterating forward from " << findKey << " ...\n";
            auto iterator = tree.at( findKey.c_str(), findKey.size() );
            for (int i = 0; i < 20; i++) {
                auto entry = *iterator++;
                stream << "Value at " << findKey << " [ " << i << " ] = " << string( entry.first.first, entry.first.second ) << " -> " << string( entry.second.first, entry.second.second ) << "\n" << flush;
            }
        }
        {
            stream << "\nIterating from begin to end...\n";
            size_t count = 0;
            for (auto iterator : tree) count += 1;
            stream << "B-Tree contains " << count << " entries\n";
        }
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
