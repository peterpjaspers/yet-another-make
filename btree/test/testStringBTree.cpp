#include "String2ValueTree.h"
#include "String2StringTree.h"
#include "Value2StringTree.h"
#include "PersistentPagePool.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <random>
#include <map>
#include <algorithm>

using namespace BTree;
using namespace std;
using namespace std::filesystem;

const int BTreePageSize = 4096;
const int ValueCount = 10000;
const int MinString = 2;
const int MaxString = 15;
const int MinArray = 2;
const int MaxArray = 15;

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
vector<uint16_t> generateUint16Array() {
    vector<uint16_t> value;
    int n = (MinArray  + (gen32() % (MaxArray - MinArray)));
    for (int i = 0; i < n; i++) value.push_back( gen32() % 10000 );
    return value;
}
void streamUint16Array( ofstream& stream, vector<uint16_t>& value ) {
    size_t n = value.size();
    stream << "[";
    if (0 < n) {
        stream << " " << value[0];
        for (int i = 1; i < n; i++) stream << ", " << value[i];
    }
    stream << " ]";
}
void streamUint16Array( ofstream& stream, const uint16_t* value, PageIndex valueSize ) {
    stream << "[";
    if (0 < valueSize) {
        stream << " " << value[0];
        for (int i = 1; i < valueSize; i++) stream << ", " << value[i];
    }
    stream << " ]";
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
    remove_all( "testStringBTree" );
    create_directory( "testStringBTree ");
    int errors = 0;
    // String2String test
    stream.open( "testStringBTree\\logStringBTree.txt" );
    try {
        map<string,string> entries;
        vector<string> keys;
        BTree::String2StringTree tree(
            *createPagePool( true, "testStringBTree\\String2String.bt", BTreePageSize )
        );
        stream << "Writing " << ValueCount << " strings with string keys...\n";
        for ( int i = 0; i < ValueCount; i++ ) {
            string key = generateString();
            while (0 < entries.count( key )) key = generateString();
            keys.push_back( key );
            string value = generateString();
            entries[ key ] = value;
            if (!tree.insert( key, value )) {
                stream << "Key " << key << " already present!\n";
                errors += 1;
            }
        }
        stream << "Modifying " << ValueCount << " strings with string keys...\n";
        for ( auto key : keys ) {
            string value = generateString();
            entries[ key ] = value;
            if (!tree.replace( key, value )) {
                stream << "Key " << key << " not present!\n";
                errors += 1;
            }
        }
        stream << "Reading " << ValueCount << " strings with string keys...\n";
        for ( auto key : keys ) {
            auto value = tree.at( key );
            if ( value != entries[ key ] ) {
                stream << "Value mismatch for key " << key << " : expected " << entries[ key ] << ", retrieved " << value << "!\n";
                errors += 1;
            }
        }
        tree.commit();
        stream << tree;
    }
    catch ( string message ) {
        stream << message << "!\n";
        errors += 1;
    }
    catch (...) {
        stream << "Exception!\n";
        errors += 1;
    }
    stream << "Done...\n";
    // String2Value test - scalar value
    try {
        map<string,uint32_t> entries;
        vector<string> keys;
        BTree::String2ValueTree<uint32_t> tree(
            *createPagePool( true, "testStringBTree\\String2Uint32.bt", BTreePageSize )
        );
        stream << "Writing " << ValueCount << " 32-bit unsigned integers with string keys...\n";
        for ( int i = 0; i < ValueCount; i++ ) {
            string key = generateString();
            while (0 < entries.count( key )) key = generateString();
            keys.push_back( key );
            uint32_t value = generateUint32();
            entries[ key ] = value;
            if (!tree.insert( key, value )) {
                stream << "Key " << key << " already present!\n";
                errors += 1;
            }
        }
        stream << "Modifying " << ValueCount << " 32-bit unsigned integers with string keys...\n";
        for ( auto key : keys ) {
            uint32_t value = generateUint32();
            entries[ key ] = value;
            if (!tree.replace( key, value )) {
                stream << "Key " << key << " not present!\n";
                errors += 1;
            }
        }
        stream << "Reading " << ValueCount << " 32-bit unsigned integers with string keys...\n";
        for ( auto key : keys ) {
            auto value = tree.at( key );
            if ( value != entries[ key ] ) {
                stream << "Value mismatch for key " << key << " : expected " << entries[ key ] << ", retrieved " << value << "!\n";
                errors += 1;
            }
        }
        tree.commit();
        stream << tree;
    }
    catch ( string message ) {
        stream << message << "!\n";
        errors += 1;
    }
    catch (...) {
        stream << "Exception!\n";
        errors += 1;
    }
    stream << "Done...\n";
    // String2Value test - array value
    try {
        vector<string> keys;
        map<string,vector<uint16_t>> entries;
        BTree::String2ValueTree<uint16_t[]> tree(
            *createPagePool( true, "testStringBTree\\String2Uint16Array.bt", BTreePageSize )
        );
        stream << "Writing " << ValueCount << " 16-bit unsigned integer arrays with string keys...\n";
        for ( int i = 0; i < ValueCount; i++ ) {
            auto key = generateString();
            while (0 < entries.count( key )) key = generateString();
            keys.push_back( key );
            auto value = generateUint16Array();
            entries[ key ] = value;
            if(!tree.insert( key, value.data(), static_cast<PageSize>(value.size()) )) {
                stream << "Key " << key << " already present!\n";
                errors += 1;
            }
        }
        stream << "Modifying " << ValueCount << " 16-bit unsigned integer arrays with string keys...\n";
        for ( auto key : keys ) {
            auto value = generateUint16Array();
            entries[ key ] = value;
            if (!tree.replace( key, value.data(), static_cast<PageSize>(value.size()) )) {
                stream << "Key " << key << " not present!\n";
                errors += 1;
            }
        }
        stream << "Reading " << ValueCount << " 16-bit unsigned integer arrays with string keys...\n";
        for ( auto key : keys ) {
            auto reference = entries[ key ];
            auto retrieved = tree.at( key );
            if (BTree::defaultCompareArray( reference.data(), static_cast<PageSize>(reference.size()), retrieved.first, static_cast<PageSize>(retrieved.second) ) != 0) {
                stream << "Value mismatch for key " << key << " : expected ";
                streamUint16Array( stream, reference );
                stream << ", retrieved ";
                streamUint16Array( stream, retrieved.first, retrieved.second );
                stream << "!\n";
                errors += 1;
            }
        }
        tree.commit();
        stream << tree;
    }
    catch ( string message ) {
        stream << message << "!\n";
        errors += 1;
    }
    catch (...) {
        stream << "Exception!\n";
        errors += 1;
    }
    stream << "Done...\n";
    // Value2String test - scalar keys
    try {
        vector<uint32_t> keys;
        map<uint32_t,string> entries;
        BTree::Value2StringTree<uint32_t> tree(
            *createPagePool( true, "testStringBTree\\Uint322String.bt", BTreePageSize )
        );
        stream << "Writing " << ValueCount << " strings with 32-bit unsigned int keys...\n";
        for ( int i = 0; i < ValueCount; i++ ) {
            auto key = generateUint32();
            while (0 < entries.count( key )) key = generateUint32();
            keys.push_back( key );
            auto value = generateString();
            entries[ key ] = value;
            if (!tree.insert( key, value )) {
                stream << "Key " << key << " already present!\n";
                errors += 1;
            }
        }
        stream << "Modifying " << ValueCount << " strings with 32-bit unsigned int keys...\n";
        for ( auto key : keys ) {
            auto value = generateString();
            entries[ key ] = value;
            if (!tree.replace( key, value )) {
                stream << "Key " << key << " not present!\n";
                errors += 1;
            }
        }
        stream << "Reading " << ValueCount << " strings with 32-bit unsigned int keys...\n";
        for ( auto key : keys ) {
            auto reference = entries[ key ];
            auto value = tree.at( key );
            if ( value != reference ) {
                stream << "Value mismatch for " << key << " : expected " << reference << ", retrieved " << value << ".\n";
                errors += 1;
            }
        }
        tree.commit();
        stream << tree;
    }
    catch ( string message ) {
        stream << message << "!\n";
        errors += 1;
    }
    catch (...) {
        stream << "Exception!\n";
        errors += 1;
    }
    stream << "Done...\n";
    // Value2String test - array keys
    try {
        vector<vector<uint16_t>> keys;
        map<vector<uint16_t>,string> entries;
        BTree::Value2StringTree<uint16_t[]> tree(
            *createPagePool( true, "testStringBTree\\Uint16Array2String.bt", BTreePageSize )
        );
        stream << "Writing " << ValueCount << " strings with 16-bit unsigned int array keys...\n";
        for ( int i = 0; i < ValueCount; i++ ) {
            auto key = generateUint16Array();
            string value = generateString();
            while ( entries.count( key ) != 0 ) key = generateUint16Array();
            entries[ key ] = value;
            if (!tree.insert( key.data(), static_cast<PageSize>(key.size()), value )) {
                stream << "Key "; streamUint16Array( stream, key ); stream << " already present!\n";
                errors += 1;
            }
        }
        stream << "Modifying " << ValueCount << " strings with 16-bit unsigned int array keys...\n";
        for ( auto key : keys ) {
            string value = generateString();
            if (!tree.replace( key.data(), static_cast<PageSize>(key.size()), value )) {
                stream << "Key "; streamUint16Array( stream, key ); stream << " not present!\n";
                errors += 1;
            }
        }
        stream << "Reading " << ValueCount << " strings with 16-bit unsigned int array keys...\n";
        for ( auto key : keys ) {
            auto reference = entries[ key ];
            auto value = tree.at( key.data(), static_cast<PageSize>(key.size()) );
            if ( value != reference ) {
                stream << "Value mismatch : expected " << reference << ", retrieved " << value << ".\n";
                errors += 1;
            }
        }
        tree.commit();
        stream << tree;
    }
    catch ( string message ) {
        stream << message << "!\n";
        errors += 1;
    }
    catch (...) {
        stream << "Exception!\n";
        errors += 1;
    }
    if (0 < errors) {
        stream << "\n\n" << errors << " errors detected.";
    } else {
        stream << "\n\nNo errors detected.\n";
    }
    stream.close();
    return( errors );
}
