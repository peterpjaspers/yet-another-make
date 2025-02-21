#ifndef BTREE_PAGE_STREAM_ASCII_H
#define BTREE_PAGE_STREAM_ASCII_H

#include "Types.h"

#include <ostream>

namespace BTree {

    namespace ASCIIStreamer {

        // Page streaming IO manipulators
        // IO stream manipulator constants
        enum Mode {
            Native = 0,
            Hex = 1,
            Dec = 2,
            ASCII = 3
        };

        static long StreamKeyIndex = std::ios_base::xalloc();
        static long StreamValueIndex = std::ios_base::xalloc();

        class keys {
        public:
            keys() : streamMode( Native ) {};
            keys( Mode m ) : streamMode( m ) {};
            static Mode mode( std::ostream& stream ) { return( static_cast<Mode>( stream.iword( StreamKeyIndex ) ) ); }
            std::ostream& operator()( std::ostream& stream ) const {
                stream.iword( StreamKeyIndex ) = static_cast<long>( streamMode );
                return stream;
            };
        private:
            Mode streamMode;
        };
        class values {
        public:
            values() : streamMode( Native ) {};
            values( Mode m ) : streamMode( m ) {};
            static Mode mode( std::ostream& stream ) { return( static_cast<Mode>( stream.iword( StreamValueIndex ) ) ); }
            std::ostream& operator()( std::ostream& stream ) const {
                stream.iword( StreamValueIndex ) = static_cast<long>( streamMode );
                return stream;
            };
        private:
            Mode streamMode;
        };

    }; 

    static std::ostream& operator<<( std::ostream& stream, ASCIIStreamer::keys mode ) { mode( stream ); return stream; }
    static std::ostream& operator<<( std::ostream& stream, ASCIIStreamer::values mode ) { mode( stream ); return stream; }

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
        // Stream page content as ASCII text to an output stream.
        // Stream in hex format if stream base is set to hex; otherwise stream in decimal format.
        template <class K, class V, bool KA, bool VA>
        friend std::ostream &operator<<(std::ostream &o, const Page<K, V, KA, VA>& page);
    private:
        template <class K, class V, bool KA, bool VA>
        static void streamPageHex(std::ostream &o, const Page<K, V, KA, VA>& page);
    }; // class ASCIIPageStreamer

    template <class K, class V, bool KA, bool VA>
    inline std::ostream &operator<<(std::ostream &o, const Page<K, V, KA, VA>& page);

}; // namespace BTRee

#include "PageStreamASCII_Imp.h"

#endif // BTREE_PAGE_STREAM_ASCII_H