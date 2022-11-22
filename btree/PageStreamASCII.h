#ifndef BTREE_PAGE_STREAM_ASCII_H
#define BTREE_PAGE_STREAM_ASCII_H

#include <algorithm>

#include <stdlib.h>
#include <cstdint>
#include <string>
#include <iostream>
#include <iomanip>

#include "Types.h"

// ToDo: Key specific streaming, pass as program argument to constructor

namespace BTree {

    class ASCIIPageStreamer {
    public:

        template <class K, class V>
        static void streamPageMapping( std::ostream &o,  const Page<K, V, false, false>& page ){
            o << page.header.count << " : " << sizeof(K) << " -> " << sizeof(V) << "\n";            
        }
        template <class K, class V>
        static void streamPageMapping( std::ostream &o, const Page<K, V, true, false>& page ){
            o << page.header.count << " : [ " << sizeof(K) << " ] -> " << sizeof(V) << "\n";            
        }
        template <class K, class V>
        static void streamPageMapping( std::ostream &o, const Page<K, V, false, true>& page ){
            o << page.header.count << " : " << sizeof(K) << " -> [ " << sizeof(V) << " ]\n";            
        }
        template <class K, class V>
        static void streamPageMapping( std::ostream &o, const Page<K, V, true, true>& page ){
            o << page.header.count << " : [ " << sizeof(K) << " ] -> [ " << sizeof(V) << " ]\n";            
        }

        template <class K, class V, bool KA, bool VA>
        static void streamPageHeader(std::ostream &o, const Page<K, V, KA, VA>& page) {
            o << "Page" << page.header.page << "." << page.header.depth << " (";
            if (page.header.persistent == 1) o << "*";
            if (page.header.modified == 1) o << "~";
            if (page.header.recover == 1) o << "!";
            if (page.header.free == 1) o << "?";
            o << ")\n";
            streamPageMapping<K,V>( o, page );
            o << "[ " << page.filling() << " / " << page.header.capacity << " ] " << std::setprecision(1) << std::fixed << ((100.0 * page.filling()) / page.header.capacity) << " %\n";
        }

        // Stream page content in human readable form.
        // Stream content as scalar keys mapping to scalar values...
        template <class K, class V>
        static void streamPage(std::ostream &o, const Page<K,V,false,false>& page) {
            streamPageHeader(o, page);
            if (page.splitDefined()) {
                o << "     - : " << page.split() << "\n";
            }
            for (int k = 0; k < page.header.count; k++) {
                o << std::setw(6) << k << " : " << page.key(k) << " -> " << page.value(k) << "\n";
            }
        }
        // Stream content as array keys mapping to scalar values...
        template <class K, class V>
        static void streamPage(std::ostream &o, const Page<K,V,true,false>& page) {
            streamPageHeader(o, page);
            if (page.splitDefined()) {
                o << "     - : " << page.split() << "\n";
            }
            for (int k = 0; k < page.header.count; k++) {
                o << std::setw(6) << k << " : [ ";
                o << page.key(k)[0];
                for (int i = 1; i < page.keySize(k); i++) o << ", " << page.key(k)[i];
                o << " ] -> " << page.value(k) << "\n";
            }
        }
        // Stream content as scalar keys mapping to array values...
        template <class K, class V>
        static void streamPage(std::ostream &o, const Page<K,V,false,true>& page) {
            streamPageHeader(o, page);
            if (page.splitDefined()) {
                o << "     - : [ " << page.split()[0];
                for (int i = 1; i < page.splitSize(); i++) o << ", " << page.split()[i];
                o << " ]\n";
            }
            for (int k = 0; k < page.header.count; k++) {
                o << std::setw(6) << k << " : " << page.key(k) << " -> [ ";
                o << page.value(k)[0];
                for (int i = 1; i < page.valueSize(k); i++) o << ", " << page.value(k)[i];
                o << " ]\n";
            }
        }
        // Stream content as array keys mapping to array values...
        template <class K, class V>
        static void streamPage(std::ostream &o, const Page<K,V,true,true>& page) {
            streamPageHeader(o, page);
            if (page.splitDefined()) {
                o << "     - : [ " << page.split()[0];
                for (int i = 1; i < page.splitSize(); i++) o << ", " << page.split()[i];
                o << " ]\n";
            }
            for (int k = 0; k < page.header.count; k++) {
                o << std::setw(6) << k << " : [ ";
                o << page.key(k)[0];
                for (int i = 1; i < page.keySize(k); i++) o << ", " << page.key(k)[i];
                o << " ] -> [ ";
                o << page.value(k)[0];
                for (int i = 1; i < page.valueSize(k); i++) o << ", " << page.value(k)[i];
                o << " ]\n";
            }
        }

        static void streamPageContentHex(std::ostream &o, const uint8_t *data, int n) {
            for (int i = 0; i < n; i++) {
                o << std::setfill('0') << std::hex << std::setw(2) << static_cast<unsigned int>(data[i]) << std::setfill(' ') << " " << std::dec;
            }
        }
        template <class K, class V, bool KA, bool VA>
        static void streamPageHex(std::ostream &o, const Page<K, V, KA, VA>& page) {
            o << std::dec;
            streamPageHeader(o, page);
            // Stream Page as hexadecimal bytes...
            int bytes = (page.header.capacity - sizeof(PageHeader));
            // Column index header...
            o << std::setw(6 + 3) << " ";
            const int BPL = 32; // Bytes Per Line
            for (int i = 0; i < std::min(bytes, BPL); i++) {
                o << std::setw(2) << i;
                if (i == ((BPL / 2) - 1)) {
                    o << " . ";
                } else {
                    o << " ";
                }
            }
            o << "\n";
            o << std::setw(6 + 3) << " ";
            for (int i = 0; i < std::min(bytes, BPL); i++) {
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
                o << std::setw(6) << (BPL * l) << " | ";
                streamPageContentHex(o, &page.content()[BPL * l], std::min((bytes - (BPL * l)), (BPL / 2)));
                if (((BPL * l) + (BPL / 2)) < bytes) {
                    o << ". ";
                    streamPageContentHex(o, &page.content()[(BPL * l) + (BPL / 2)], std::min((bytes - ((BPL * l) + (BPL / 2))), (BPL / 2)));
                }
                o << "\n" << std::dec;
            }
        }

    }; // class ASCIIPageStreamer

    // Stream page content as ASCII text to an output stream
    template <class K, class V, bool KA, bool VA>
    inline std::ostream &operator<<(std::ostream &o, const Page<K, V, KA, VA>& page) {
        if (o.flags() & std::ios_base::hex)
            ASCIIPageStreamer::streamPageHex(o, page);
        else
            ASCIIPageStreamer::streamPage(o, page);
        return o;
    };

}; // namespace BTRee

#endif // BTREE_PAGE_STREAM_ASCII_H