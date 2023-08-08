#ifndef VALUE_STREAMER_H
#define VALUE_STREAMER_H

#include "StreamingBTree.h"

namespace BTree {

    typedef std::uint16_t StreamBlockSize;
    typedef std::uint16_t StreamSequence;
    template< class K >
    struct StreamKey {
        K key;
        StreamSequence sequence;
        StreamKey() : sequence( UINT16_MAX ) {};
        StreamKey( const K& k, StreamSequence s ) : key( k ), sequence( s ) {};
        StreamKey( const StreamKey<K>& range ) : key( range.key ), sequence( range.sequence ) {};
    };
    template< class K >
    std::ostream & operator<<( std::ostream & stream, StreamKey<K> const & range ) {
        stream << "[ " << range.key << " : " << range.sequence << " ]";
        return stream;
    };
    

    template< class K >
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
        virtual ValueStreamer<K>& open( const K& streamKey ) = 0;
        virtual bool isOpen() const = 0;
        virtual void close() = 0;
    };

    template< class K >
    class ValueReader : public ValueStreamer<K> {
    private:
        const Tree<StreamKey<K>,uint8_t[]>& tree;
        StreamKey<K> key;
        std::pair<const uint8_t*, PageSize> buffer;
        uint16_t position;
        void readBlock() {
            if ( buffer.second <= position ) {
                buffer = tree.retrieve( key );
                key.sequence += 1;
                position = 0;
            }
        }
        uint8_t getByte() {
            static const char* signature( "uint8_t ValueReader::getByte()" );
            readBlock();
            if ( buffer.first == nullptr ) throw std::string( signature ) + " - Streaming beyond end of stream";
            return buffer.first[ position++ ];
        }
        template <class T>
        void getValue( T& value ) {
           uint8_t* streamed = static_cast<uint8_t*>( static_cast<void*>( &value ) );
           for (size_t i = 0; i < sizeof( T ); i++) streamed[ i ] = getByte();
        }
    public:
        ValueReader( const Tree<StreamKey<K>,uint8_t[]>& values ) :
            tree( values ), buffer( nullptr, UINT16_MAX ), position( UINT16_MAX )
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
        ValueReader<K>& open( const K& streamKey ) {
            static const char* signature = "ValueReader& ValueReader::open( const StreamKey<K>& streamKey )";
            if (isOpen()) throw std::string( signature ) + " - Opening while reader open";
            key = StreamKey<K>( streamKey, 0 );
            return *this;
        }
        bool isOpen() const { return( position != UINT16_MAX ); }
        void close() {
            static const char* signature = "void ValueReader::close()";
            if (!isOpen()) throw std::string( signature ) + " - Closing closed reader";
            position = UINT16_MAX;
            
        }
    }; // class ValueReader

    template< class K >
    class ValueWriter : public ValueStreamer<K> {
    private:
        Tree<StreamKey<K>,uint8_t[]>& tree;
        StreamKey<K> key;
        std::vector<uint8_t> buffer;
        uint16_t position;
        void writeBlock() {
            static const char* signature( "void ValueWriter::writeBlock()" );
            if (0 < position) {
                if (key.sequence == UINT16_MAX) throw std::string( signature ) + " - Exceding maximum block count";
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
        ValueWriter( Tree<StreamKey<K>,uint8_t[]>& values, StreamBlockSize block ) :
            tree( values ), buffer( block ), position( UINT16_MAX )
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
        ValueWriter<K>& open( const K& streamKey ) {
            static const char* signature = "ValueWriter& ValueWriter::open( const StreamKey<K>& streamKey )";
            if (isOpen()) throw std::string( signature ) + " - Opening while writer open";
            key = StreamKey<K>( streamKey, 0 );
            position = 0;
            return *this;
        }
        bool isOpen() const { return( position != UINT16_MAX ); }
        void close() {
            static const char* signature = "void ValueWriter::close()";
            if (!isOpen()) throw std::string( signature ) + " - Closing closed writer";
            writeBlock();
            position = UINT16_MAX;
        }
    }; // class ValueWriter

}; // namespace BTree

#endif // VALUE_STREAMER_H