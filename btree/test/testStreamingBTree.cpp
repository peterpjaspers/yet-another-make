#include "StreamingBTree.h"
#include "StreamingBTreeIterator.h"
#include "PersistentPagePool.h"
#include <iostream>
#include <fstream>

using namespace BTree;
using namespace std;

const int BTreePageSize = 4096;
const int ValueBlockSize = 128;
const int ObjectCount = 10;

// ToDo: Generate multiple keys with multiple objects per key...

struct Object {
    bool b;
    float f;
    double d;
    uint8_t u8;
    int8_t i8;
    uint16_t u16;
    int16_t i16;
    uint32_t u32;
    int32_t i32;
    uint64_t u64;
    int64_t i64;
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

template< class K >
void streamObject( ValueStreamer<K>& s, Object& o ) {
    s.stream( o.b );
    s.stream( o.f );
    s.stream( o.d );
    s.stream( o.u8 );
    s.stream( o.i8 );
    s.stream( o.u16 );
    s.stream( o.i16 );
    s.stream( o.u32 );
    s.stream( o.i32 );
    s.stream( o.u64 );
    s.stream( o.i64 );
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

const uint32_t KeyCount = 3;
uint32_t keys[ KeyCount ] = { 47, 37, 137 };

int main(int argc, char* argv[]) {
    system( "RMDIR /S /Q testStreamingBTree" );
    system( "MKDIR testStreamingBTree" );
    ofstream log;
    log.open( "testStreamingBTree\\log.txt" );
    try {
        BTree::StreamingTree<uint32_t> tree( *createPagePool( true, "testStreamingBTree\\StreamingBTree.bt", BTreePageSize ) );
        log << "Writing " << KeyCount << " sets of " << ObjectCount << " objects...\n";
        for (int i = 0; i < KeyCount; ++i) {
            log << "Writing " << ObjectCount << " objects at key " << keys[ i ] << ".\n";
            ValueWriter<uint32_t>& writer = tree.insert( keys[ i ] );
            for ( uint16_t c = 0; c < ObjectCount; c++ ) {
                Object o( keys[ i ] + c );
                streamObject<uint32_t>( writer, o );
            }
            writer.close();
        }
        tree.commit();
        log << hex << tree;
        log << "Reading " << KeyCount << " sets of " << ObjectCount << " objects...\n";
        for (int i = 0; i < KeyCount; ++i) {
            log << "Reading " << ObjectCount << " objects at key " << keys[ i ] << ".\n";
            ValueReader<uint32_t>& reader = tree.retrieve( keys[ i ] );
            for ( uint16_t c = 0; c < ObjectCount; c++ ) {
                Object o( 0 ), r( keys[ i ] + c );
                streamObject<uint32_t>( reader, o );
                if ( o != r ) log << "Value mismatch at key " << keys[ i ] << ", object " << c << ".\n";
            }
            reader.close();
        }
        log << "Iterator tests...\n";
        int count = 0;
        for ( auto& reader : tree ) {
            uint32_t key = reader.key();
            log << "Reading " << ObjectCount << " objects at key " << key << ".\n";
            for ( uint16_t c = 0; c < ObjectCount; c++ ) {
                Object o( 0 ), r( key + c );
                streamObject<uint32_t>( reader, o );
                if ( o != r ) log << "Value mismatch at key " << key << ", object " << c << ".\n";
            }
            reader.close();
            count += 1;
        }
        if ( count != KeyCount ) log << "Iterator count mismatch : Expected " << KeyCount << ", actual " << count << "!\n";
    }
    catch ( string message ) {
        log << message << "\n";
    }
    catch (...) {
        log << "Exception!\n";
    }
    log << "Done...\n";
    log.close();
};
