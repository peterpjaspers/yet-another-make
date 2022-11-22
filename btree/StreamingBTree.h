#ifndef STREAMING_BTREE_BTREE_H
#define STREAMING_BTREE_BTREE_H

#include "BTree.h"

namespace BTree {

    class ValueStreamer {
        virtual void stream( bool& value ) = 0;
        virtual void stream( float& value ) = 0;
        virtual void stream( double& value ) = 0;
        virtual void stream( int8_t& value ) = 0;
        virtual void stream( uint8_t& value ) = 0;
        virtual void stream( int16_t& value ) = 0;
        virtual void stream( uint16_t& value ) = 0;
        virtual void stream( int32_t& value ) = 0;
        virtual void stream( uint32_t& value ) = 0;
        virtual void stream( int64_t& value ) = 0;
        virtual void stream( uint64_t& value ) = 0;
        virtual void close() = 0;
    };
    class ValueReader {
        void stream( bool& value );
        void stream( float& value );
        void stream( double& value );
        void stream( int8_t& value );
        void stream( uint8_t& value );
        void stream( int16_t& value );
        void stream( uint16_t& value );
        void stream( int32_t& value );
        void stream( uint32_t& value );
        void stream( int64_t& value );
        void stream( uint64_t& value );
        void close();
    };
    class ValueWriter {
        void stream( bool& value );
        void stream( float& value );
        void stream( double& value );
        void stream( int8_t& value );
        void stream( uint8_t& value );
        void stream( int16_t& value );
        void stream( uint16_t& value );
        void stream( int32_t& value );
        void stream( uint32_t& value );
        void stream( int64_t& value );
        void stream( uint64_t& value );
        void close();
    };

    template< class K, bool KA >
    class StreamingTree {
        Tree<K,uint64_t,KA,false>& keys;
        struct KeyRange {
            uint64_t key;
            uint16_t sequence;
        };
        Tree<KeyRange,uint8_t,false,true>& values;
    public:
        StreamingTree( PagePool& pool, UpdateMode mode = Auto );
        template<bool AK = KA>
        typename std::enable_if<(!AK),ValueWriter>::type& insert( const K& key );
        template<bool AK = KA>
        typename std::enable_if<(!AK),ValueReader>::type& retrieve( const K& key );
        template<bool AK = KA>
        typename std::enable_if<(!AK),void>::type& remove( const K& key );
        template<bool AK = KA>
        typename std::enable_if<(AK),ValueWriter>::type& insert( const K& key, PageSize keySize );
        template<bool AK = KA>
        typename std::enable_if<(AK),ValueReader>::type& retrieve( const K& key, PageSize keySize );
        template<bool AK = KA>
        typename std::enable_if<(AK),void>::type& remove( const K& key, PageSize keySize );
        void commit() { keys.commit(); values.commit(); };
        void recover(){ keys.recover(); values.recover(); };
    };

}; // namespace BTree

#endif // STREAMING_BTREE_BTREE_H