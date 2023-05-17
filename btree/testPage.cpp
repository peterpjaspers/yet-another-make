#include <cstring>
#include <cstdlib>
#include <chrono>
#include <iostream>
#include <fstream>

#include "Page.h"
#include "PagePool.h"

using namespace std;
using namespace BTree;

// ToDo: Add tests for copy on update behavior.
// ToDo: exchange tests

const int PageCapacity = 4096;

const bool ScalarScalarPage = true;
const bool ArrayScalarPage = true;
const bool ScalarArrayPage = true;
const bool ArrayArrayPage = true;

char* generateName( int n ) {
    char* name = static_cast<char*>( malloc( n + 1 ) );
    const char alfanum[ 36 ] = {
        'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
        'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'\
    };
    for (int i = 0; i < n; i += 1) {
        name[ i ] = alfanum[ rand() % 36 ];
    }
    name[ n ] = 0;
    return name;
}

#define VALIDATE 0
#define MAX_N 1000000
#define NODE_POOL_CAPACITY 1000

int compare( const int& a, const int& b ) { return(a - b); };

int main(int argc, char* argv[]) {
    int n = MAX_N;
    if (1 < argc) { n = min( stoi( argv[ 1] ), MAX_N ); }
    string fileName( "testPage.txt" );
    if (2 < argc) { fileName = argv[2]; }
    ofstream stream;
    stream.open( fileName );

    try {
        PagePool pool(PageCapacity);
        // Create a number of pages
        PageLink pages[ 20 ];
        for (int i = 0; i < 20; i++) { pages[ i ] = pool.allocate()->page; }

    if (ScalarScalarPage) {

        stream << "Creating uint16_t Node with random (unsorted) keys...\n";
        Page<uint16_t,PageLink,false,false>& node = *pool.page<uint16_t,PageLink,false,false>( 1 );
        for (int i = 0; i < n; i++) {
            node.insert( (rand() % (node.header.count + 1)), uint16_t(rand() % 10000), pages[ i % 20 ] );
        }
        stream << "Filled...\n";
        stream << node;
        Page<uint16_t,PageLink,false,false>& updated = *pool.page<uint16_t,PageLink,false,false>( 1 );
        stream << "Copy on update insert...\n";
        node.insert( (rand() % (node.header.count + 1)), uint16_t(rand() % 10000), pages[ 13 ], &updated );
        stream << updated;
        stream << "Copy on update split add...\n";
        node.split( pages[ rand() % 20 ], &updated );
        stream << updated;
        stream << "Copy on update replace...\n";
        node.replace( (rand() % node.header.count), pages[ rand() % 20 ], &updated );
        stream << updated;
        stream << "Copy on update remove...\n";
        node.remove( (rand() % node.header.count), &updated );
        stream << updated;
        stream << "Added split value...\n";
        node.split( pages[ rand() % 20 ] );
        stream << node;
        Page<uint16_t,PageLink,false,false>& left = *pool.page<uint16_t,PageLink,false,false>( 1 );
        node.shiftLeft( left, (node.header.count / 4) );
        stream << "Shifted left...\n";
        stream << node;
        stream << "Left...\n";
        stream << left;
        Page<uint16_t,PageLink,false,false>& updatedLeft = *pool.page<uint16_t,PageLink,false,false>( 1 );
        stream << "Copy on update shift left...\n";
        node.shiftLeft( left, (node.header.count / 4), &updated, &updatedLeft );
        stream << updated;
        stream << "Updated left...\n";
        stream << updatedLeft;
        Page<uint16_t,PageLink,false,false>& right = *pool.page<uint16_t,PageLink,false,false>( 1 );
        node.shiftRight( right, (3 * node.header.count / 4) );
        stream << "Shifted right...\n";
        stream << node;
        stream << "Right...\n";
        stream << right;
        Page<uint16_t,PageLink,false,false>& updatedRight = *pool.page<uint16_t,PageLink,false,false>( 1 );
        stream << "Copy on update shift right...\n";
        node.shiftRight( right, (3 * node.header.count / 4), &updated, &updatedRight );
        stream << updated;
        stream << "Updated right...\n";
        stream << updatedRight;
        node.shiftLeft( left, (node.header.count / 4) );
        stream << "Shifted left again...\n";
        stream << node;
        stream << "Left again...\n";
        stream << left;
        node.shiftRight( right, (3 * node.header.count / 4) );
        stream << "Shifted right again...\n";
        stream << node;
        stream << "Right again...\n";
        stream << right;
        int nr = min<int>( n, node.header.count );
        stream << "Replacing " << nr << " entries ...\n";
        for (int i = 0; i < nr; i++) {
            node.replace( i, pages[ rand() % 20 ] );
        }
        stream << "Replaced...\n";
        stream << node;
        nr = min<int>( n, node.header.count );
        stream << "Removing " << nr << " entries ...\n";
        for (int i = 0; i < nr; i++) {
            node.remove( rand() % node.header.count );
        }
        stream << node;
        stream << hex << node;
    }

    if (ArrayScalarPage) {
        stream << "Creating C-string Node with random (unsorted) keys...\n";
        Page<char,PageLink,true,false>& node = *pool.page<char,PageLink,true,false>( 1 );
        for (int i = 0; i < n; i++) {
            char* key = generateName( ((rand() % 10) + 2) );
            node.insert( (rand() % (node.header.count + 1)), key, strlen( key ), pages[ i % 20 ] );
        }
        stream << "Filled...\n";
        stream << node;
        Page<char,PageLink,true,false>& updated = *pool.page<char,PageLink,true,false>( 1 );
        stream << "Copy on update insert...\n";
        char* key = generateName( ((rand() % 10) + 2) );
        node.insert( (rand() % (node.header.count + 1)), key, strlen( key ), pages[ 13 ], &updated );
        stream << updated;
        stream << "Copy on update split add...\n";
        node.split( pages[ rand() % 20 ], &updated );
        stream << updated;
        stream << "Copy on update replace...\n";
        node.replace( (rand() % node.header.count), pages[ rand() % 20 ], &updated );
        stream << updated;
        stream << "Copy on update remove...\n";
        node.remove( (rand() % node.header.count), &updated );
        stream << updated;
        node.split( pages[ rand() % 20 ] );
        stream << "Added split value...\n";
        stream << node;
        Page<char,PageLink,true,false>& left = *pool.page<char,PageLink,true,false>( 1 );
        node.shiftLeft( left, (node.header.count / 4) );
        stream << "Shifted left...\n";
        stream << node;
        stream << "Left...\n";
        stream << left;
        Page<char,PageLink,true,false>& updatedLeft = *pool.page<char,PageLink,true,false>( 1 );
        stream << "Copy on update shift left...\n";
        node.shiftLeft( left, (node.header.count / 4), &updated, &updatedLeft );
        stream << updated;
        stream << "Updated left...\n";
        stream << updatedLeft;
        Page<char,PageLink,true,false>& right = *pool.page<char,PageLink,true,false>( 1 );
        node.shiftRight( right, (3 * node.header.count / 4) );
        stream << "Shifted right...\n";
        stream << node;
        stream << "Right...\n";
        stream << right;
        Page<char,PageLink,true,false>& updatedRight = *pool.page<char,PageLink,true,false>( 1 );
        stream << "Copy on update shift right...\n";
        node.shiftRight( right, (3 * node.header.count / 4), &updated, &updatedRight );
        stream << updated;
        stream << "Updated right...\n";
        stream << updatedRight;
        stream << "Shifted left again...\n";
        node.shiftLeft( left, (node.header.count / 4) );
        stream << node;
        stream << "Left again...\n";
        stream << left;
        stream << "Shifted right again...\n";
        node.shiftRight( right, (3 * node.header.count / 4) );
        stream << node;
        stream << "Right again...\n";
        stream << right;
        int nr = min<int>( n, node.header.count );
        stream << "Replacing " << nr << " entries ...\n";
        for (int i = 0; i < nr; i++) {
            node.replace( i, pages[ rand() % 20 ] );
        }
        stream << "Replaced...\n";
        stream << node;
        nr = min<int>( n, node.header.count );
        stream << "Removing " << nr << " entries ...\n";
        for (int i = 0; i < nr; i++) {
            node.remove( rand() % node.header.count );
        }
        stream << "Removed...\n";
        stream << node;
        stream << hex << node;

    }

    if (ScalarArrayPage) {
        stream << "Creating PageLink to C-string Node with random (unsorted) keys...\n";
        Page<PageLink,char,false,true>& node = *pool.page<PageLink,char,false,true>( 1 );
        for (int i = 0; i < n; i++) {
            char* value = generateName( ((rand() % 10) + 2) );
            node.insert( (rand() % (node.header.count + 1)), pages[ i % 20 ], value, strlen( value ) );
        }
        stream << "Filled...\n";
        stream << node;
        stream << "Copy on update insert...\n";
        Page<PageLink,char,false,true>& updated = *pool.page<PageLink,char,false,true>( 1 );
        char* value = generateName( ((rand() % 10) + 2) );
        node.insert( (rand() % (node.header.count + 1)), pages[ 13 ], value, strlen( value ), &updated );
        stream << updated;
        stream << "Copy on update split add...\n";
        char* splitValue = generateName( ((rand() % 10) + 2) );
        node.split( splitValue, strlen( splitValue), &updated );
        stream << updated;
        stream << "Copy on update replace...\n";
        value = generateName( ((rand() % 10) + 2) );
        node.replace( (rand() % node.header.count), value, strlen( value ), &updated );
        stream << updated;
        stream << "Copy on update remove...\n";
        node.remove( (rand() % node.header.count), &updated );
        stream << updated;
        stream << "Added split value...\n";
        splitValue = generateName( ((rand() % 10) + 2) );
        node.split( splitValue, strlen( splitValue ) );
        stream << node;
        Page<PageLink,char,false,true>& left = *pool.page<PageLink,char,false,true>( 1 );
        node.shiftLeft( left, (node.header.count / 4) );
        stream << "Shifted left...\n";
        stream << node;
        stream << "Left...\n";
        stream << left;
        Page<PageLink,char,false,true>& updatedLeft = *pool.page<PageLink,char,false,true>( 1 );
        stream << "Copy on update shift left...\n";
        node.shiftLeft( left, (node.header.count / 4), &updated, &updatedLeft );
        stream << updated;
        stream << "Updated left...\n";
        stream << updatedLeft;
        Page<PageLink,char,false,true>& right = *pool.page<PageLink,char,false,true>( 1 );
        stream << "Shifted right...\n";
        node.shiftRight( right, (3 * node.header.count / 4) );
        stream << node;
        stream << "Right...\n";
        stream << right;
        Page<PageLink,char,false,true>& updatedRight = *pool.page<PageLink,char,false,true>( 1 );
        stream << "Copy on update shift right...\n";
        node.shiftRight( right, (3 * node.header.count / 4), &updated, &updatedRight );
        stream << updated;
        stream << "Updated right...\n";
        stream << updatedRight;
        node.shiftLeft( left, (node.header.count / 4) );
        stream << "Shifted left again...\n";
        stream << node;
        stream << "Left again...\n";
        stream << left;
        node.shiftRight( right, (3 * node.header.count / 4) );
        stream << "Shifted right again...\n";
        stream << node;
        stream << "Right again...\n";
        stream << right;
        int nr = min<int>( n, node.header.count );
        stream << "Replacing " << nr << " entries ...\n";
        for (int i = 0; i < nr; i++) {
            char* value = generateName( ((rand() % 10) + 2) );
            node.replace( i, value, strlen( value ) );
        }
        stream << "Replaced...\n";
        stream << node;
        nr = min<int>( n, node.header.count );
        stream << "Removing " << nr << " entries ...\n";
        for (int i = 0; i < nr; i++) {
            node.remove( rand() % node.header.count );
        }
        stream << "Removed...\n";
        stream << node;
        stream << hex << node;
    }

    if (ArrayArrayPage) {
        stream << "Creating C-string to C-string page with random (unsorted) keys...\n";
        Page<char,char,true,true>& node = *pool.page<char,char,true,true>( 1 );
        for (int i = 0; i < n; i++) {
            char* key = generateName( ((rand() % 10) + 2) );
            char* value = generateName( ((rand() % 10) + 2) );
            node.insert( (rand() % (node.header.count + 1)), key, strlen( key ), value, strlen( value ) );
        }
        stream << "Filled...\n";
        stream << node;
        Page<char,char,true,true>& updated = *pool.page<char,char,true,true>( 1 );
        stream << "Copy on update insert...\n";
        char* key = generateName( ((rand() % 10) + 2) );
        char* value = generateName( ((rand() % 10) + 2) );
        node.insert( (rand() % (node.header.count + 1)), key, strlen( key ), value, strlen( value ), &updated );
        stream << updated;
        stream << "Copy on update split add...\n";
        value = generateName( ((rand() % 10) + 2) );
        node.split( value, strlen( value ), &updated );
        stream << updated;
        stream << "Copy on update replace...\n";
        value = generateName( ((rand() % 10) + 2) );
        node.replace( (rand() % node.header.count), value, strlen( value ), &updated );
        stream << updated;
        stream << "Copy on update remove...\n";
        node.remove( (rand() % node.header.count), &updated );
        stream << updated;
        stream << "Added split value...\n";
        value = generateName( ((rand() % 10) + 2) );
        node.split( value, strlen( value ) );
        stream << node;
        Page<char,char,true,true>& left = *pool.page<char,char,true,true>( 1 );
        node.shiftLeft( left, (node.header.count / 4) );
        stream << "Shifted left...\n";
        stream << node;
        stream << "Left...\n";
        stream << left;
        Page<char,char,true,true>& updatedLeft = *pool.page<char,char,true,true>( 1 );
        stream << "Copy on update shift left...\n";
        node.shiftLeft( left, (node.header.count / 4), &updated, &updatedLeft );
        stream << updated;
        stream << "Updated left...\n";
        stream << updatedLeft;
        Page<char,char,true,true>& right = *pool.page<char,char,true,true>( 1 );
        node.shiftRight( right, (3 * node.header.count / 4) );
        stream << "Shifted right...\n";
        stream << node;
        stream << "Right...\n";
        stream << right;
        Page<char,char,true,true>& updatedRight = *pool.page<char,char,true,true>( 1 );
        stream << "Copy on update shift right...\n";
        node.shiftRight( right, (3 * node.header.count / 4), &updated, &updatedRight );
        stream << updated;
        stream << "Updated right...\n";
        stream << updatedRight;
        stream << "Shifted left again...\n";
        node.shiftLeft( left, (node.header.count / 4) );
        stream << node;
        stream << "Left again...\n";
        stream << left;
        node.shiftRight( right, (3 * node.header.count / 4) );
        stream << "Shifted right again...\n";
        stream << node;
        stream << "Right again...\n";
        stream << right;
        int nr = min<int>( n, node.header.count );
        stream << "Replacing " << nr << " entries ...\n";
        for (int i = 0; i < nr; i++) {
            char* value = generateName( ((rand() % 10) + 2) );
            node.replace( i, value, strlen( value ) );
        }
        stream << "Replaced...\n";
        stream << node;
        nr = min<int>( n, node.header.count );
        stream << "Removing " << nr << " entries ...\n";
        for (int i = 0; i < nr; i++) {
            node.remove( rand() % node.header.count );
        }
        stream << "Removed...\n";
        stream << node;
        stream << hex << node;
    }

    }
    catch ( char const* message ) {
        stream << message << "\n";
    }
    catch (...) {
        stream << "Exception!\n";
    }
    stream << "Done...\n";
}