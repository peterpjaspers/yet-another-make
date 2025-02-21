#ifndef BTREE_PAGE_STREAM_ASCII_IMP_H
#define BTREE_PAGE_STREAM_ASCII_IMP_H

#include "PageStreamASCII.h"
#include <cmath>
#include <iomanip>

using namespace std;

namespace BTree {

    namespace {

        template <class K, class V>
        void streamPageMapping( ostream &o,  const Page<K, V, false, false>& page ){
            o << page.header.count << " : " << sizeof(K) << " -> " << sizeof(V) << "\n";            
        }
        template <class K, class V>
        void streamPageMapping( ostream &o, const Page<K, V, true, false>& page ){
            o << page.header.count << " : [ " << sizeof(K) << " ] -> " << sizeof(V) << "\n";            
        }
        template <class K, class V>
        void streamPageMapping( ostream &o, const Page<K, V, false, true>& page ){
            o << page.header.count << " : " << sizeof(K) << " -> [ " << sizeof(V) << " ]\n";            
        }
        template <class K, class V>
        void streamPageMapping( ostream &o, const Page<K, V, true, true>& page ){
            o << page.header.count << " : [ " << sizeof(K) << " ] -> [ " << sizeof(V) << " ]\n";            
        }

        template <class K, class V, bool KA, bool VA>
        void streamPageHeader(ostream &o, const Page<K, V, KA, VA>& page) {
            o << "Page" << page.header.page << "." << page.header.depth << " (";
            if (page.header.free == 1) o << "F";
            if (page.header.modified == 1) o << "M";
            if (page.header.persistent == 1) o << "P";
            if (page.header.recover == 1) o << "R";
            if (page.header.stored == 1) o << "S";
            o << ")\n";
            streamPageMapping<K,V>( o, page );
            o << "[ " << page.filling() << " / " << page.header.capacity << " ] " << setprecision(1) << fixed << ((100.0 * page.filling()) / page.header.capacity) << " %\n";
        }

        // Arrays of keys or value are streamed in aligned blocks.
        const unsigned int ArrayBlockSize( 160 ); // Block size in humand readable characters
        template< class T >
        struct BlockStreamer{
            typedef void (*Function)( ostream &o, const T* data, int n, int digits, const char* separator );
        };
        const char ToASCII[ 256 ] = {
            // 0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F
              ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', // 0
              ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', // 1
              ' ', '!', '"', '#', '$', '%', '&','\'', '(', ')', '*', '+', ',', '-', '.', '/', // 2
              '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?', // 3
              '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', // 4
              'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[','\\', ']', '^', '_', // 5
              '`', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', // 6
              'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|', '}', '~', ' ', // 7
              ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', // 8
              ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', // 9
              ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', // A
              ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', // B
              ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', // C
              ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', // D
              ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', // E
              ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', // F
        };
        template< class T >
        void streamArrayBlockASCII( ostream &o, const T* data, int n, int digits, const char* separator ) {
            o.flags( ios_base::dec );
            if (0 < n) o << ToASCII[ data[0] ];
            for (int i = 1; i < n; i++) o << separator << ToASCII[ data[i] ];
        }
        template< class T >
        void streamArrayBlockDec( ostream &o, const T* data, int n, int digits, const char* separator ) {
            o.flags( ios_base::dec );
            if (0 < n) o << setw(digits) << static_cast<int64_t>( data[0] );
            for (int i = 1; i < n; i++) o << separator << setw(digits) << static_cast<int64_t>( data[i] );
        }
        template< class T >
        void streamArrayBlockHex( ostream &o, const T* data, int n, int digits, const char* separator ) {
            o.flags( ios_base::hex );
            if (0 < n) o << setw(digits) << static_cast<int64_t>( data[0] );
            for (int i = 1; i < n; i++) o << separator << setw(digits) << static_cast<int64_t>( data[i] );
            o.flags( ios_base::dec );
        }
        template< class T >
        void streamArrayBlock( ostream &o, const T* data, int n, int digits, const char* separator ) {
            if (0 < n) o << data[0];
            for (int i = 1; i < n; i++) o << separator << data[i];
        }
        template< class T >
        void streamArray( ostream &o, const T* data, int n, typename BlockStreamer<T>::Function block, int blockSize, int digits, const char* separator ) {
            o << n << " [ ";
            if (n <= blockSize) {
                block( o, data, n, digits, separator ); 
            } else {
                int remaining( n );
                while (blockSize < remaining) {
                    o << "\n       "; block( o, data, blockSize, digits, separator );
                    remaining -= blockSize;
                    data += blockSize;
                    o << ",";
                }
                o << "\n       "; block( o, data, remaining, digits, separator ); o << "\n";
            }
            o << " ]";
        }
        template< class T >
        void streamArrayValue(ostream &o, ASCIIStreamer::Mode mode, const T* data, int n) {
            if (mode == ASCIIStreamer::ASCII) {
                streamArray<T>( o, data, n, streamArrayBlockASCII, 120, 1, "" );
            } else if (mode == ASCIIStreamer::Hex) {
                auto digits( static_cast<int>( ceil( sizeof(T) * 2.0 ) ) );
                auto blockSize( ArrayBlockSize / (digits + 2) );
                streamArray<T>( o, data, n, streamArrayBlockHex, blockSize, digits, " " );
            } else if (mode == ASCIIStreamer::Dec) {
                auto digits( static_cast<int>( ceil( (sizeof(T) * 8) / 3.3219 ) ) );
                auto blockSize( ArrayBlockSize / (digits + 2) );
                streamArray<T>( o, data, n, streamArrayBlockDec, blockSize, digits, " " );
            } else {
                streamArray<T>( o, data, n, streamArrayBlock, n, 0, ", " );
            }
        }
        template< class T >
        void streamPageContentHex(ostream &o, const T* data, int n) {
            for (int i = 0; i < n; i++) {
                o << setfill('0') << ios_base::hex << setw(2) << static_cast<unsigned int>(data[i]) << setfill(' ') << " " << ios_base::dec;
            }
        }
    }

    template <class K, class V, bool KA, bool VA>
    void ASCIIPageStreamer::streamPageHex(ostream &o, const Page<K, V, KA, VA>& page) {
        o << ios_base::dec;
        streamPageHeader(o, page);
        // Stream Page as hexadecimal bytes...
        int bytes = (page.header.capacity - sizeof(PageHeader));
        // Column index header...
        o << setw(6 + 3) << " ";
        const int BPL = 32; // Bytes Per Line
        for (int i = 0; i < min(bytes, BPL); i++) {
            o << setw(2) << i;
            if (i == ((BPL / 2) - 1)) {
                o << " . ";
            } else {
                o << " ";
            }
        }
        o << "\n";
        o << setw(6 + 3) << " ";
        for (int i = 0; i < min(bytes, BPL); i++) {
            o << "--";
            if (i == ((BPL / 2) - 1)) {
                o << " . ";
            } else {
                o << " ";
            }
        }
        o << "\n";
        // Actual content in lines of 16 bytes...
        for (int l = 0; l < ((bytes + (BPL - 1)) / BPL); l++) {
            o << setw(6) << (BPL * l) << " | ";
            streamPageContentHex<uint8_t>(o, &page.content()[BPL * l], min((bytes - (BPL * l)), (BPL / 2)));
            if (((BPL * l) + (BPL / 2)) < bytes) {
                o << ". ";
                streamPageContentHex<uint8_t>(o, &page.content()[(BPL * l) + (BPL / 2)], min((bytes - ((BPL * l) + (BPL / 2))), (BPL / 2)));
            }
            o << "\n" << ios_base::dec;
        }
    }

    // ToDo: Stream 8-bit numeric types as hex (not ASCII chracters)

    template <class K, class V>
    void ASCIIPageStreamer::streamPage(ostream &o, const Page<K,V,false,false>& page) {
        streamPageHeader(o, page);
        if (page.splitDefined()) {
            o << "     - : " << page.split() << "\n";
        }
        for (int k = 0; k < page.header.count; k++) {
            o << setw(6) << k << " : " << page.key(k) << " -> " << page.value(k) << "\n";
        }
    }
    template <class K, class V>
    void ASCIIPageStreamer::streamPage(ostream &o, const Page<K,V,true,false>& page) {
        streamPageHeader(o, page);
        if (page.splitDefined()) {
            o << "     - : " << page.split() << "\n";
        }
        for (int k = 0; k < page.header.count; k++) {
            o << setw(6) << k << " : ";
            streamArrayValue<K>( o, ASCIIStreamer::keys::mode( o ), page.key(k), page.keySize(k) );
            o << " -> " << page.value(k) << "\n";
        }
    }
    template <class K, class V>
    void ASCIIPageStreamer::streamPage(ostream &o, const Page<K,V,false,true>& page) {
        streamPageHeader(o, page);
        if (page.splitDefined()) {
            o << "     - : ";
            streamArrayValue<V>( o, ASCIIStreamer::values::mode( o ), page.split(), page.splitSize() );
            o << "\n";
        }
        for (int k = 0; k < page.header.count; k++) {
            o << setw(6) << k << " : " << page.key(k) << " -> ";
            streamArrayValue<V>( o, ASCIIStreamer::values::mode( o ), page.value(k), page.valueSize(k) );
            o << "\n";
        }
    }
    template <class K, class V>
    void ASCIIPageStreamer::streamPage(ostream &o, const Page<K,V,true,true>& page) {
        streamPageHeader(o, page);
        if (page.splitDefined()) {
            o << "     - : ";
            streamArrayValue<V>( o, ASCIIStreamer::values::mode( o ), page.split(), page.splitSize() );
            o << "\n";
        }
        for (int k = 0; k < page.header.count; k++) {
            o << setw(6) << k << " : ";
            streamArrayValue<K>( o, ASCIIStreamer::keys::mode( o ), page.key(k), page.keySize(k) );
            o << " -> ";
            streamArrayValue<V>( o, ASCIIStreamer::values::mode( o ), page.value(k), page.valueSize(k) );
            o << "\n";
        }
    }

    template <class K, class V, bool KA, bool VA>
    inline ostream &operator<<(ostream &o, const Page<K, V, KA, VA>& page) {
        if (o.flags() & ios_base::hex) ASCIIPageStreamer::streamPageHex(o, page);
        else ASCIIPageStreamer::streamPage(o, page);
        return o;
    };

}; // namespace BTRee

#endif // BTREE_PAGE_STREAM_ASCII_IMP_H