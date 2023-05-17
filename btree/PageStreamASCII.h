#ifndef BTREE_PAGE_STREAM_ASCII_H
#define BTREE_PAGE_STREAM_ASCII_H

#include <stdlib.h>
#include <cstdint>
#include <string.h>
#include <iostream>
#include <iomanip>

#include "Types.h"

namespace BTree {

    class ASCIIPageStreamer {
    public:
        // Stream page content in human readable form.
        // Stream content as scalar keys mapping to scalar values...
        template <class K, class V>
        static void streamPage(std::ostream &o, const Page<K,V,false,false>& page);
        // Stream content as array keys mapping to scalar values...
        template <class K, class V>
        static void streamPage(std::ostream &o, const Page<K,V,true,false>& page);
        // Stream content as scalar keys mapping to array values...
        template <class K, class V>
        static void streamPage(std::ostream &o, const Page<K,V,false,true>& page);
        // Stream content as array keys mapping to array values...
        template <class K, class V>
        static void streamPage(std::ostream &o, const Page<K,V,true,true>& page);
        template <class K, class V, bool KA, bool VA>
        static void streamPageHex(std::ostream &o, const Page<K, V, KA, VA>& page);
    private:
        template <class K, class V>
        static void streamPageMapping( std::ostream &o,  const Page<K, V, false, false>& page );
        template <class K, class V>
        static void streamPageMapping( std::ostream &o, const Page<K, V, true, false>& page );
        template <class K, class V>
        static void streamPageMapping( std::ostream &o, const Page<K, V, false, true>& page );
        template <class K, class V>
        static void streamPageMapping( std::ostream &o, const Page<K, V, true, true>& page );
        template <class K, class V, bool KA, bool VA>
        static void streamPageHeader(std::ostream &o, const Page<K, V, KA, VA>& page);
        template< class T >
        static void streamPageContentHex(std::ostream &o, const T *data, int n);
    }; // class ASCIIPageStreamer

    // Stream page content as ASCII text to an output stream.
    // Stream in hex format if stream base is set to hex; otherwise stream in decimal format.
    template <class K, class V, bool KA, bool VA>
    inline std::ostream &operator<<(std::ostream &o, const Page<K, V, KA, VA>& page);

}; // namespace BTRee

#include "PageStreamASCII_Imp.h"

#endif // BTREE_PAGE_STREAM_ASCII_H