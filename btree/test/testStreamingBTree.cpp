#include "String2StreamBTree.h"
#include "PersistentPagePool.h"
#include <iostream>
#include <fstream>

using namespace BTree;
using namespace std;

const int BTreePageSize = 4096;
const int ValueBlockSize = 128;
const int ObjectCount = 10;

// ToDo: Generate multiple keys with multiple objects per key...

class Object {
    bool b;
    float f;
    double d;
    uint8_t u8;
    int8_t i8;
    uint8_t u16;
    int8_t i16;
    uint8_t u32;
    int8_t i32;
    uint8_t u64;
    int8_t i64;
public:
    Object() = delete;
    Object( uint64_t x ) {
        b = ((x & 1) == 0) ? true : false;
        f = x;
        d = x;
        u8 = (x % (static_cast<uint64_t>( 1 ) << 8));
        i8 = ((x & (static_cast<uint64_t>( 1 ) << 8)) - (static_cast<uint64_t>( 1 ) << 7));
        u16 = (x % (static_cast<uint64_t>( 1 ) << 16));
        i16 = ((x & (static_cast<uint64_t>( 1 ) << 16)) - (static_cast<uint64_t>( 1 ) << 15));
        u32 = (x % (static_cast<uint64_t>( 1 ) << 32));
        i32 = ((x & (static_cast<uint64_t>( 1 ) << 32)) - (static_cast<uint64_t>( 1 ) << 31));
        u64 = x;
        i64 = (x - (static_cast<uint64_t>( 1 ) << 63));
    }
    void stream( ValueStreamer& s ) {
        s.stream( b );
        s.stream( f );
        s.stream( d );
        s.stream( u8 );
        s.stream( i8 );
        s.stream( u16 );
        s.stream( i16 );
        s.stream( u32 );
        s.stream( i32 );
        s.stream( u64 );
        s.stream( i64 );
    }
    friend bool operator== ( const Object& a, const Object& b );
};

bool operator== ( const Object& a, const Object& b ) {
    if (a.b != b.b) return false;
    if (a.f != b.f) return false;
    if (a.d != b.d) return false;
    if (a.u8 != b.u8) return false;
    if (a.i8 != b.i8) return false;
    if (a.u16 != b.u16) return false;
    if (a.i16 != b.i16) return false;
    if (a.u32 != b.u32) return false;
    if (a.i32 != b.i32) return false;
    if (a.u64 != b.u64) return false;
    if (a.i64 != b.i64) return false;
    return true;
}
bool operator!= ( const Object& a, const Object& b ) { return( (a == b) ? false : true ); }

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
    system( "RMDIR /S /Q testStreamingBTree" );
    system( "MKDIR testStreamingBTree" );
    ofstream stream;
    stream.open( "testStreamingBTree\\testStreamingBTree.txt" );
    try {
        BTree::String2StreamTree tree(
            *createPagePool( true, "testStreamingBTree\\String2IndexBTree.bt", BTreePageSize ),
            *createPagePool( true, "testStreamingBTree\\Index2StreamBTree.bt", BTreePageSize ),
            ValueBlockSize
        );
        stream << "Writing " << ObjectCount << " objects...\n";
        string key = "An object";
        ValueWriter& writer = tree.insert( key );
        for ( uint16_t i = 0; i < ObjectCount; i++ ) {
            Object o( i );
            o.stream( writer );
        }
        writer.close();
        tree.commit();
        if (ObjectCount <= 10) stream << tree;
        stream << "Reading " << ObjectCount << " objects...\n";
        ValueReader& reader = tree.retrieve( key );
        for ( uint16_t i = 0; i < ObjectCount; i++ ) {
            Object o( 0 ), r( i );
            o.stream( reader );
            if ( o != r ) stream << "Value mismatch at " << i << ".\n";
        }
        reader.close();
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
