#ifndef VALUE_STREAMER_H
#define VALUE_STREAMER_H

#include "BTree.h"

namespace BTree {

    class ValueStreamer {
    public:
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
        virtual bool eos() = 0;
        virtual void close() = 0;
    };

    class ValueReader : public ValueStreamer {
    private:
        const Tree<StreamKey,uint8_t,false,true>& tree;
        StreamKey key;
        std::pair<const uint8_t*, PageIndex> buffer;
        uint16_t position;
        void readBlock() {
            if ( position == buffer.second ) {
                buffer = tree.retrieve( key );
                key.sequence += 1;
                position = 0;
            }
        }
        uint8_t getByte() {
            static const std::string signature( "uint8_t ValueReader::getByte()" );
            readBlock();
            if ( buffer.first == nullptr ) throw signature + " - Streaming beyond end of stream";
            return buffer.first[ position++ ];
        }
        template <class T>
        void getValue( T& value ) {
           uint8_t* streamed = static_cast<uint8_t*>( static_cast<void*>( &value ) );
           for (size_t i = 0; i < sizeof( T ); i++) streamed[ i ] = getByte();
        }
    public:
        ValueReader( const Tree<StreamKey,uint8_t,false,true>& values, StreamIndex index ) :
            tree( values ), key( index, 0 ), buffer( nullptr, INT16_MAX ), position( INT16_MAX )
        {}
        void stream( bool& value ) { return( getValue<bool>( value ) ); }
        void stream( float& value ) { return( getValue<float>( value ) ); }
        void stream( double& value ) { return( getValue<double>( value ) ); }
        void stream( int8_t& value ) { return( getValue<int8_t>( value ) ); }
        void stream( uint8_t& value ) { return( getValue<uint8_t>( value ) ); }
        void stream( int16_t& value ) { return( getValue<int16_t>( value ) ); }
        void stream( uint16_t& value ) { return( getValue<uint16_t>( value ) ); }
        void stream( int32_t& value ) { return( getValue<int32_t>( value ) ); }
        void stream( uint32_t& value ) { return( getValue<uint32_t>( value ) ); }
        void stream( int64_t& value ) { return( getValue<int64_t>( value ) ); }
        void stream( uint64_t& value ) { return( getValue<uint64_t>( value ) ); }
        bool eos() { readBlock(); return( buffer.first == nullptr ); }
        void close() {}
    }; // class ValueReader

    class ValueWriter : public ValueStreamer {
    private:
        Tree<StreamKey,uint8_t,false,true>& tree;
        StreamKey key;
        std::vector<uint8_t> buffer;
        uint16_t position;
        void writeBlock() {
            static const std::string signature( "void ValueWriter::writeBlock()" );
            if (0 < position) {
                if (key.sequence == UINT16_MAX) throw signature + " - Exceding maximum block count";
                tree.insert( key, &buffer[ 0 ], position );
                key.sequence += 1;
                position = 0;
            }
        }
        void putByte( const uint8_t value ) {
            buffer[ position++ ] = value;
            if ( position == buffer.size() ) writeBlock();
        }
        template <class T>
        void putValue( T& value ) {
           uint8_t* streamed = static_cast<uint8_t*>( static_cast<void*>( &value ) );
           for (size_t i = 0; i < sizeof( T ); i++) putByte( streamed[ i ] );
        }
    public:
        ValueWriter( Tree<StreamKey,uint8_t,false,true>& values, StreamIndex index, StreamBlockSize block ) :
            tree( values ), key( index, 0 ), buffer( block ), position( 0 )
        {}
        void stream( bool& value ) { return( putValue<bool>( value ) ); }
        void stream( float& value ) { return( putValue<float>( value ) ); }
        void stream( double& value ) { return( putValue<double>( value ) ); }
        void stream( int8_t& value ) { return( putValue<int8_t>( value ) ); }
        void stream( uint8_t& value ) { return( putValue<uint8_t>( value ) ); }
        void stream( int16_t& value ) { return( putValue<int16_t>( value ) ); }
        void stream( uint16_t& value ) { return( putValue<uint16_t>( value ) ); }
        void stream( int32_t& value ) { return( putValue<int32_t>( value ) ); }
        void stream( uint32_t& value ) { return( putValue<uint32_t>( value ) ); }
        void stream( int64_t& value ) { return( putValue<int64_t>( value ) ); }
        void stream( uint64_t& value ) { return( putValue<uint64_t>( value ) ); }
        bool eos() { return( false ); }
        void close() { writeBlock(); }
    }; // class ValueWriter

}; // namespace BTree

#endif // VALUE_STREAMER_H