#ifndef BTREE_PAGE_H
#define BTREE_PAGE_H

#include <new>
#include <stdlib.h>
#include <cstdint>

#include "Types.h"
#include "PageFunctions.h"
#include "PageStreamASCII.h"

namespace BTree {

    // Template arguments:
    //
    //    class K   Object class of keys
    //    class V   Object class of values
    //    bool KA   Keys are arrays of K
    //    bool VA   Values are arrays of V
    //
    // K and V must be of fixed size; i.e., eligible as argument to 'sizeof' operator.
    template <class K, class V, bool KA, bool VA>
    class Page {
    public:
        PageHeader header;
        Page() = delete;
        Page( const PageDepth depth ) {
            header.depth = depth;
            clear();
        }
        inline bool empty() const { return ((header.count == 0) && (header.split == 0)); }
        friend class PageFunctions;
        friend class ASCIIPageStreamer;
    private:

        // Data in pages is organized as follows:
        //
        // Fixed size key to fixed size value page layout:
        //   header        : Page type information
        //   split         : Fixed size split value (if any)
        //   keys          : Fixed size key data
        //   values        : Fixed size value data
        // Variable size key to fixed size value page layout:
        //   header        : Page type information
        //   split         : Fixed size split value (if any)
        //   values        : Fixed size value data
        //   key indeces   : Cumulative size of variable size keys, index of key in key data
        //   key values    : Variable size key data
        // Fixed size key to variable size value page layout:
        //   header        : Page type information
        //   split         : Variable size split value (if any)
        //   keys          : Fixed size key data
        //   value indeces : Cumulative size of variable size values, index of value in value data
        //   value data    : Variable size value data
        // Variable size key to variable size value page layout:
        //   header        : Page type information
        //   split         : Variable size split value (if any)
        //   key indeces   : Cumulative size of variable size keys, index of key in key data
        //   value indeces : Cumulative size of variable size values, index of value in value data
        //   key data      : Variable size key data
        //   value data    : Variable size value data
        //

        inline uint8_t* content() const {
            return ((reinterpret_cast<uint8_t*>(const_cast<PageHeader *>(&header))) + sizeof(PageHeader));
        }
        template <bool AK = KA>
        inline typename std::enable_if_t<(!AK), K> *keys() const {
            return (reinterpret_cast<K*>(const_cast<uint8_t *>(&content()[header.split * sizeof(V)])));
        }
        template <bool AK = KA, bool AV = VA, std::enable_if_t<(AK&&!AV), bool> = true>
        inline K* keys() const {
            return (reinterpret_cast<K*>(&keyIndeces()[header.count]));
        }
        template <bool AK = KA, bool AV = VA, std::enable_if_t<(AK&&AV), bool> = true>
        inline K* keys() const {
            return (reinterpret_cast<K*>(&valueIndeces()[header.count]));
        }
        template <bool AK = KA, bool AV = VA, std::enable_if_t<(AK&&!AV), bool> = true>
        inline PageSize* keyIndeces() const {
            return (reinterpret_cast<PageSize*>(&values()[header.count]));
        }
        template <bool AK = KA, bool AV = VA,std::enable_if_t<(AK&&AV), bool> = true>
        inline PageSize* keyIndeces() const {
            return (reinterpret_cast<PageSize*>(const_cast<uint8_t *>(&content()[header.split * sizeof(V)])));
        }
        template <bool AK = KA, std::enable_if_t<(AK), bool> = true>
        inline PageIndex keyIndex(PageIndex index) const {
            return ((index == 0) ? 0 : keyIndeces()[index - 1]);
        }
        template <bool AK = KA, bool AV = VA, std::enable_if_t<(!AK&&!AV), bool> = true>
        inline V* values() const {
            return (reinterpret_cast<V*>(&keys()[header.count]));
        }
        template <bool AK = KA, bool AV = VA, std::enable_if_t<(AK&&!AV), bool> = true>
        inline V* values() const {
            return (reinterpret_cast<V*>(const_cast<uint8_t *>(&content()[header.split * sizeof(V)])));
        }
        template <bool AK = KA, bool AV = VA, std::enable_if_t<(AK&&AV), bool> = true>
        inline V* values() const {
            return (reinterpret_cast<V*>(const_cast<K*>(&keys()[keyIndex(header.count)])));
        }
        template <bool AK = KA, bool AV = VA, std::enable_if_t<(!AK&&AV), bool> = true>
        inline V* values() const {
            return (reinterpret_cast<V*>(&valueIndeces()[header.count]));
        }
        template <bool AK = KA, bool AV = VA, std::enable_if_t<(!AK&&AV), bool> = true>
        inline PageSize* valueIndeces() const {
            return (reinterpret_cast<PageSize *>(&keys()[header.count]));
        }
        template <bool AK = KA, bool AV = VA,std::enable_if_t<(AK&&AV), bool> = true>
        inline PageSize* valueIndeces() const {
            return (reinterpret_cast<PageSize *>(&keyIndeces()[header.count]));
        }
        template <bool AV = VA, std::enable_if_t<(AV), bool> = true>
        inline PageIndex valueIndex(PageIndex index) const {
            return ((index == 0) ? 0 : valueIndeces()[index - 1]);
        }
        template <bool AV = VA, std::enable_if_t<(!AV), bool> = true>
        inline V& splitValue() const {
            return (*reinterpret_cast<V*>(const_cast<uint8_t *>(content())));
        }
        template <bool AV = VA, std::enable_if_t<(AV), bool> = true>
        inline V* splitValue() const {
            return (reinterpret_cast<V*>(const_cast<uint8_t *>(content())));
        }
        template< bool AK = KA, bool AV = VA, std::enable_if_t<(!AK&&!AV), bool> = true >
        inline void initialize() {}
        template< bool AK = KA, bool AV = VA, std::enable_if_t<(AK&&!AV), bool> = true >
        inline void initialize() { keyIndeces()[ 0 ] = 0;  }
        template< bool AK = KA, bool AV = VA, std::enable_if_t<(!AK&&AV), bool> = true >
        inline void initialize() { valueIndeces()[ 0 ] = 0; }
        template< bool AK = KA, bool AV = VA, std::enable_if_t<(AK&&AV), bool> = true >
        inline void initialize() { keyIndeces()[ 0 ] = 0; valueIndeces()[ 0 ] = 0; }

    public:
        // Return required page content storage from first entry up to requested index.
        template< bool AK = KA, bool AV = VA, std::enable_if_t<(!AK&&!AV),bool> = true >
        inline PageSize indexedFilling( PageIndex index ) const {
            return (index * (sizeof(K) + sizeof(V)));
        }
        template< bool AK = KA, bool AV = VA, std::enable_if_t<(AK&&!AV),bool> = true >
        inline PageSize indexedFilling( PageIndex index ) const {
            return (index * (sizeof(V) + sizeof(PageIndex))) + (keyIndex<AK>(index) * sizeof(K));
        }
        template< bool AK = KA, bool AV = VA, std::enable_if_t<(!AK&&AV),bool> = true >
        inline PageSize indexedFilling( PageIndex index ) const {
            return (index * (sizeof(K) + sizeof(PageIndex))) + (valueIndex<AV>(index) * sizeof(V));
        }
        template< bool AK = KA, bool AV = VA, std::enable_if_t<(AK&&AV),bool> = true >
        inline PageSize indexedFilling( PageIndex index ) const {
            return (2 * index * sizeof(PageIndex)) + (keyIndex<AK>(index) * sizeof(K)) + (valueIndex<AV>(index) * sizeof(V));
        }
        // Return required page content storage up to the requested index.
        template< bool AK = KA, bool AV = VA >
        inline PageSize filling( PageIndex index ) const {
            return (sizeof(PageHeader) + (header.split * sizeof(V)) + indexedFilling<AK,AV>( index ));
        }
        // Return required page content storage for entire page content.
        inline PageSize filling() const { return filling<KA,VA>( header.count ); };

        // Return number of bytes required to store a single key-value entry.
        template< bool AK = KA, bool AV = VA, std::enable_if_t<(!AK&&!AV),bool> = true >
        inline PageSize entryFilling() const {
            return (sizeof(K) + sizeof(V));
        }
        template< bool AK = KA, bool AV = VA, std::enable_if_t<(AK&&!AV),bool> = true >
        inline PageSize entryFilling(const PageSize keySize) const {
            return (sizeof(PageSize) + (keySize * sizeof(K)) + sizeof(V));
        }
        template< bool AK = KA, bool AV = VA, std::enable_if_t<(!AK&&AV),bool> = true >
        inline PageSize entryFilling(const PageSize valueSize) const {
            return (sizeof(K) + sizeof(PageSize) + (valueSize * sizeof(V)));
        }
        template< bool AK = KA, bool AV = VA, std::enable_if_t<(AK&&AV),bool> = true >
        inline PageSize entryFilling(const PageSize keySize, const PageSize valueSize) const {
            return ((2 * sizeof(PageSize)) + (keySize * sizeof(K)) + (valueSize * sizeof(V)));
        }


        // Determine if an entry will fit in this page.
        template< bool AK = KA, bool AV = VA, std::enable_if_t<(!AK&&!AV),bool> = true >
        inline bool entryFit() const {
            if ((filling() + entryFilling()) <= header.capacity) return true;
            return false;
        }
        template< bool AK = KA, bool AV = VA, std::enable_if_t<(AK&&!AV),bool> = true >
        inline bool entryFit( const PageSize keySize ) const {
            if ((filling() + entryFilling( keySize )) <= header.capacity) return true;
            return false;
        }
        template< bool AK = KA, bool AV = VA, std::enable_if_t<(!AK&&AV),bool> = true >
        inline bool entryFit( const PageSize valueSize ) const {
            if ((filling() + entryFilling( valueSize )) <= header.capacity) return true;
            return false;
        }
        template< bool AK = KA, bool AV = VA, std::enable_if_t<(AK&&AV),bool> = true >
        inline bool entryFit( const PageSize keySize, const PageSize valueSize ) const {
            if ((filling() + entryFilling( keySize, valueSize )) <= header.capacity) return true;
            return false;
        }

        // Empties the page, removing all key-value entries and split value.
        void clear() {
            header.count = 0;
            header.split = 0;
            initialize<KA,VA>();
        }
        // Return number of entries in this page.
        inline PageIndex size() const { return header.count; }

        // Return reference to scalar key at index.
        template <bool AK = KA, std::enable_if_t<(!AK),bool> = true>
        const K& key(PageIndex index) const {
            static const char* signature( "const K& Page<K,V,false,VA>::ey( PageIndex index ) const" );
            if (header.count <= index) throw std::string( signature ) + " - Invalid index";
            return (keys()[index]);
        }
        // Return pointer to array key at index.
        template <bool AK = KA, std::enable_if_t<(AK),bool> = true>
        const K* key(PageIndex index) const {
            static const char* signature( "const K* Page<K,V,true,VA>::key( PageIndex index ) const" );
            if (header.count <= index) throw std::string( signature ) + " - Invalid index";
            return (&keys()[keyIndex(index)]);
        }
        // Return size of array key at index.
        template <bool AK = KA, std::enable_if_t<(AK),bool> = true>
        PageSize keySize( PageIndex index ) const {
            static const char* signature( "PageSize Page<K,V,true,VA>::keySize( PageIndex index ) const" );
            if (header.count <= index) throw std::string( signature ) + " - Invalid index";
            if (index == 0) return keyIndeces()[0];
            return (keyIndeces()[index] - keyIndeces()[index - 1]);
        }
        // Return reference to scalar value at index.
        template <bool AV = VA, std::enable_if_t<(!AV),bool> = true>
        const V& value(PageIndex index) const {
            static const char* signature( "const V& Page<K,V,KA,false>::value( PageIndex index ) const" );
            if (header.count <= index) throw std::string( signature ) + " - Invalid index";
            return (values()[index]);
        }
        // Return pointer to array value at index.
        template <bool AV = VA, std::enable_if_t<(AV),bool> = true>
        const V* value( PageIndex index ) const {
            static const char* signature( "const V* Page<K,V,KA,true>::value( PageIndex index ) const" );
            if (header.count <= index) throw std::string( signature ) + " - Invalid index";
            return (&values()[valueIndex(index)]);
        }
        // Return size of array value at index.
        template <bool AV = VA, std::enable_if_t<(AV),bool> = true>
        PageSize valueSize( PageIndex index ) const {
            static const char* signature( "PageSize Page<K,V,KA,true>::valueSize( PageIndex index ) const" );
            if (header.count <= index) throw std::string( signature ) + " - Invalid index";
            if (index == 0) return (valueIndeces()[0]);
            return (valueIndeces()[index] - valueIndeces()[index - 1]);
        }

        // Test if split value is defined for this page
        bool splitDefined() const { return (header.split != 0); }
        // Return the scalar split value (if any)
        template <bool AV = VA, std::enable_if_t<(!AV),bool> = true>
        const V& split() const {
            static const char* signature( "const V& Page<K,V,KA,false>::split() const" );
            if (header.split == 0) throw std::string( signature ) + " - No split defined";
            return splitValue<false>();
        }
        // Return pointer to array split value (if any)
        template <bool AV = VA, std::enable_if_t<(AV),bool> = true>
        const V* split() const {
            static const char* signature( "const V* Page<K,V,KA,true>::split() const" );
            if (header.split == 0) throw std::string( signature ) + " - No split defined";
            return splitValue<true>();
        }
        // Return size of scalar or array split value.
        // Returns zero if no split defined.
        PageSize splitSize() const { return (header.split); }
        // Return content size of scalar or array split value.
        // Returns zero if no split defined.
        PageSize splitValueSize() const { return (header.split * sizeof(V)); }

        // Set scalar split value
        template <bool AV = VA, std::enable_if_t<(!AV),bool> = true>
        void split( const V& value, Page<K,V,KA,false>* copy )  const {
            static const char* signature( "void Page<K,V,KA,false>::split( const V& value, Page<K,V,KA,false>* copy ) const" );
            if ((header.split == 0) && (header.capacity < (filling() + sizeof(V)))) {
                throw std::string( signature ) + " - Overflow";
            }
            PageFunctions::pageSplit( *this, *copy, value );
        }
        template <bool AV = VA, std::enable_if_t<(!AV),bool> = true>
        inline void split( const V& value ) { split( value, this ); }
         // Set array split value
        template <bool AV = VA, std::enable_if_t<(AV),bool> = true>
        void split( const V* value, PageSize valueSize, Page<K,V,KA,true>* copy ) const {
            static const char* signature( "void Page<K,V,KA,true>::split( const V* value, PageSize valueSize, Page<K,V,KA,true>* copy ) const" );
            if ((header.split < valueSize) && (header.capacity < (filling() + (valueSize - header.split) * sizeof(V)))) {
                throw std::string( signature ) + " - Overflow";
            }
            PageFunctions::pageSplit( *this, *copy, value, valueSize );
        }
        template <bool AV = VA, std::enable_if_t<(AV),bool> = true>
        inline void split( const V* value, PageSize valueSize ) { split( value, valueSize, this ); }

        // Remove split value in scalar key - scalar value page
        void removeSplit( Page<K,V,KA,VA>* copy ) const {
            static const char* signature( "void Page<K,V,KA,VA>::removeSplit( Page<K,V,KA,VA>* copy ) const" );
            if (header.split == 0) throw std::string( signature ) + " - No split defined";
            PageFunctions::pageRemoveSplit( *this, *copy );
        }
        inline void removeSplit() { removeSplit( this ); }

        // Insert an scalar key - scalar value entry at an index
        template <bool AK = KA, bool AV = VA, std::enable_if_t<(!AK&&!AV), bool> = true>
        void insert( PageIndex index, const K& key, const V& value, Page<K,V,false,false>* copy ) const {
            static const char* signature( "void Page<K,V,false,false>::insert( PageIndex index, const K& key, const V& value, Page<K,V,false,false>* copy ) const" );
            if (header.count < index) throw std::string( signature ) + " - Invalid index";
            if (header.capacity < (filling() + entryFilling())) throw std::string( signature ) + " - Overflow";
            PageFunctions::pageInsert( *this, *copy, index, key, value );
        }
        template <bool AK = KA, bool AV = VA, std::enable_if_t<(!AK&&!AV), bool> = true>
        inline void insert( PageIndex index, const K& key, const V& value ) {
            insert( index, key, value, this );
        }
        // Insert an array key - scalar value entry at an index
        template <bool AK = KA, bool AV = VA, std::enable_if_t<(AK&&!AV), bool> = true>
        void insert( PageIndex index, const K* key, const PageSize keySize, const V& value, Page<K,V,true,false>* copy ) const {
            static const char* signature( "void Page<K,V,true,false>::insert( PageIndex index, const K* key, const PageSize keySize, const V& value, Page<K,V,true,false>* copy ) const" );
            if (header.count < index) throw std::string( signature ) + " - Invalid index";
            if (keySize == 0) throw std::string( signature ) + " - Invalid key size";
            if (header.capacity < (filling() + entryFilling( keySize ))) throw std::string( signature ) + " - Overflow";
            PageFunctions::pageInsert( *this, *copy, index, key, keySize, value );
        }
        template <bool AK = KA, bool AV = VA, std::enable_if_t<(AK&&!AV), bool> = true>
        inline void insert( PageIndex index, const K* key, const PageSize keySize, const V& value ) {
            insert( index, key, keySize, value, this );
        }
        // Insert a scalar key - array value entry at an index
        template <bool AK = KA, bool AV = VA, std::enable_if_t<(!AK&&AV), bool> = true>
        void insert( PageIndex index, const K& key, const V* value, const PageSize valueSize, Page<K,V,false,true>* copy ) const {
            static const char* signature( "void Page<K,V,false,true>::insert( PageIndex index, const K& key, const V* value, const PageSize valueSize, Page<K,V,false,true>* copy ) const" );
            if (header.count < index) throw std::string( signature ) + " - Invalid index";
            if (valueSize == 0) throw std::string( signature ) + " - Invalid value size";
            if (header.capacity < (filling() + entryFilling( valueSize ))) throw std::string( signature ) + " - Overflow";
            PageFunctions::pageInsert( *this, *copy, index, key, value, valueSize );
        }
        template <bool AK = KA, bool AV = VA, std::enable_if_t<(!AK&&AV), bool> = true>
        inline void insert( PageIndex index, const K& key, const V* value, const PageSize valueSize ) {
            insert( index, key, value, valueSize, this );
        }
        // Insert an array key - array value entry at an index
        template <bool AK = KA, bool AV = VA, std::enable_if_t<(AK&&AV), bool> = true>
        void insert( PageIndex index, const K* key, const PageSize keySize, const V* value, const PageSize valueSize, Page<K,V,true,true>* copy ) const {
            static const char* signature( "void Page<K,V,true,true>::insert( PageIndex index, const K* key, const PageSize keySize, const V* value, const PageSize valueSize, Page<K,V,true,true>* copy ) const" );
            if (header.count < index) throw std::string( signature ) + " - Invalid index";
            if (keySize == 0) throw std::string( signature ) + " - Invalid key size";
            if (valueSize == 0) throw std::string( signature ) + " - Invalid value size";
            if (header.capacity < (filling() + entryFilling( keySize, valueSize ))) throw std::string( signature ) + " - Overflow";
            PageFunctions::pageInsert( *this, *copy, index, key, keySize, value, valueSize );
        }
        template <bool AK = KA, bool AV = VA, std::enable_if_t<(AK&&AV), bool> = true>
        inline void insert( PageIndex index, const K* key, const PageSize keySize, const V* value, const PageSize valueSize ) {
            insert( index, key, keySize, value, valueSize, this );
        }
        
        // Replace a value at an index
        // Replace a scalar value
        template <bool AV = VA, std::enable_if_t<(!AV),bool> = true>
        void replace( PageIndex index, const V& value, Page<K,V,KA,false>* copy ) const {
            static const char* signature( "void Page<K,V,KA,false>::replace( PageIndex index, const V& value, Page<K,V,KA,false>* copy ) const" );
            if (header.count < index) throw std::string( signature ) + " - Invalid index";
            PageFunctions::pageReplace( *this, *copy, index, value );
        }
        template <bool AV = VA, std::enable_if_t<(!AV),bool> = true>
        inline void replace( PageIndex index, const V& value ) {
            replace( index, value, this );
        }
        // Replace an array value at an index
        template <bool AV = VA, std::enable_if_t<(AV),bool> = true>
        void replace( PageIndex index, const V* value, const PageSize valueSize, Page<K,V,KA,true>* copy ) const {
            static const char* signature( "void Page<K,V,KA,true>::replace( PageIndex index, const V* value, const PageSize valueSize, Page<K,V,KA,true>* copy ) const" );
            if (valueSize == 0) throw std::string( signature ) + " - Invalid value size";
            if (header.count < index) throw std::string( signature ) + " - Invalid index";
            if ((this->valueSize(index) < valueSize) && (header.capacity < (filling() + ((valueSize - this->valueSize(index)) * sizeof(V))))) {
                throw std::string( signature ) + " - Overflow";
            }
            PageFunctions::pageReplace( *this, *copy, index, value, valueSize );
        }
        template <bool AV = VA, std::enable_if_t<(AV),bool> = true>
        inline void replace( PageIndex index, const V* value, const PageSize valueSize ) {
            replace( index, value, valueSize, this );
        }
        // Replace a key-value entry at an index
        // Replace a scalar key scalar value entry
        template <bool AK = KA, bool AV = VA, std::enable_if_t<(!AK&&!AV), bool> = true>
        void replace(PageIndex index, const K& key, const V& value, Page<K,V,false,false>* copy ) const {
            static const char* signature( "void Page<K,V,false,false>::replace( PageIndex index, const K& key, const V* value, const PageSize valueSize, Page<K,V,false,false>* copy ) const" );
            if (header.count < index) throw std::string( signature ) + " - Invalid index";
            PageFunctions::pageReplace( *this, *copy, index, key, value );
        }
        template <bool AK = KA, bool AV = VA, std::enable_if_t<(!AK&&!AV), bool> = true>
        inline void replace(PageIndex index, const K& key, const V& value ) {
            replace( index, key, value, this );
        }
        // Replace an array key scalar value entry
        template <bool AK = KA, bool AV = VA, std::enable_if_t<(AK&&!AV), bool> = true>
        void replace( PageIndex index, const K* key, const PageSize keySize, const V& value, Page<K,V,true,false>* copy ) const {
            static const char* signature( "void Page<K,V,true,false>::replace( PageIndex index, const K* key, const PageSize keySize, const V& value, Page<K,V,true,false>* copy ) const" );
            if (header.count < index) throw std::string( signature ) + " - Invalid index";
            if (keySize == 0) throw std::string( signature ) + " - Invalid key size";
            if ((this->keySize(index) < keySize) && (header.capacity < (filling() + (keySize - this->keySize(index)) * sizeof(K)))) throw std::string( signature ) + " - Overflow";
            PageFunctions::pageReplace( *this, *copy, index, key, keySize, value );
        }
        template <bool AK = KA, bool AV = VA, std::enable_if_t<(AK&&!AV), bool> = true>
        inline void replace( PageIndex index, const K* key, const PageSize keySize, const V& value ) {
            replace( index, key, keySize, value, this );
        }
        // Replace a scalar key array value entry
        template <bool AK = KA, bool AV = VA, std::enable_if_t<(!AK&&AV), bool> = true>
        void replace( PageIndex index, const K& key, const V* value, const PageSize valueSize, Page<K,V,false,true>* copy ) const {
            static const char* signature( "void Page<K,V,false,true>::replace( PageIndex index, const K& key, const V* value, const PageSize valueSize, Page<K,V,false,true>* copy ) const" );
            if (header.count < index) throw std::string( signature ) + " - Invalid index";
            if (valueSize == 0) throw std::string( signature ) + " - Invalid value size";
            if ((this->valueSize(index) < valueSize) && (header.capacity < (filling() + (valueSize - this->valueSize(index))*sizeof(V)))) throw std::string( signature ) + " - Overflow";
            PageFunctions::pageReplace( *this, *copy, index, key, value, valueSize );
        }
        template <bool AK = KA, bool AV = VA, std::enable_if_t<(!AK&&AV), bool> = true>
        inline void replace( PageIndex index, const K& key, const V* value, const PageSize valueSize ) {
            replace( index, key, value, valueSize, this );
        }
        // Replace an array key array value entry
        template <bool AK = KA, bool AV = VA, std::enable_if_t<(AK&&AV), bool> = true>
        void replace( PageIndex index, const K* key, const PageSize keySize, const V* value, const PageSize valueSize, Page<K,V,true,true>* copy ) const {
            static const char* signature( "void Page<K,V,true,true>::replace( PageIndex index, const K* key, const PageSize keySize, const V* value, const PageSize valueSize, Page<K,V,true,true>* copy ) const" ) ;
            if (header.count < index) throw std::string( signature ) + " - Invalid index";
            if (keySize == 0) throw std::string( signature ) + " - Invalid key size";
            if (valueSize == 0) throw std::string( signature ) + " - Invalid value size";
            if (
                (entryFilling( this->keySize(index), this->valueSize(index) ) - entryFilling( keySize, valueSize )) &&
                (header.capacity < (filling() + entryFilling( keySize, valueSize ) - entryFilling( this->keySize(index), this->valueSize(index) )))
            ) throw std::string( signature ) + " - Overflow";
            PageFunctions::pageReplace( *this, *copy, index, key, keySize, value, valueSize );
        }
        template <bool AK = KA, bool AV = VA, std::enable_if_t<(AK&&AV), bool> = true>
        inline void replace( PageIndex index, const K* key, const PageSize keySize, const V* value, const PageSize valueSize ) {
            replace( index, key, keySize, value, valueSize, this );
        }

        // Remove the key - value entry at an index
        void remove( PageIndex index, Page<K,V,KA,VA>* copy ) const {
            static const char* signature( "void Page<K,V,KA,VA>::remove( PageIndex index, Page<K,V,KA,VA>* copy ) const" );
            if (header.count <= index) throw std::string( signature ) + " - Invalid index";
            PageFunctions::pageRemove( *this, *copy, index );
        }
        inline void remove( PageIndex index ) { remove( index, this ); }

        // Shift page content from indexed entry up to last entry to lower entries in another page
        void shiftRight( Page<K,V,KA,VA>& right, PageIndex index, Page<K,V,KA,VA>* copy, Page<K,V,KA,VA>* copyRight ) const {
            static const char* signature( "void Page<K,V,KA,VA>::shiftRight( Page<K,V,KA,VA>& right, PageIndex index, Page<K,V,KA,VA>* copy, Page<K,V,KA,VA>* copyRight ) const" );
            if (header.count < index) throw std::string( signature ) + " - Invalid index";
            if (right.header.capacity < (right.filling() + (indexedFilling( header.count ) - indexedFilling( index )))) throw std::string( signature ) + " - Overflow";
            PageFunctions::pageShiftRight( *this, right, *copy, *copyRight, index );
        }
        inline void shiftRight( Page<K,V,KA,VA>& right, PageIndex index ) {
            shiftRight( right, index, this, &right );
        }
        // Shift page content from first entry up to indexed entry up to higher entries in another page
        void shiftLeft( Page& left, PageIndex index, Page* copy, Page* copyLeft ) const {
            static const char* signature( "void Page<K,V,KA,VA>::shiftLeft( Page<K,V,KA,VA>& left, PageIndex index, Page<K,V,KA,VA>* copy, Page<K,V,KA,VA>* copyLeft ) const" );
            if (header.count < index) throw std::string( signature ) + " - Invalid index";
            if (left.header.capacity < (left.filling() + indexedFilling( index ))) throw std::string( signature ) + " - Overflow";
            PageFunctions::pageShiftLeft( *this, left, *copy, *copyLeft, index );
        }
        inline void shiftLeft( Page& left, PageIndex index ) {
            shiftLeft( left, index, this, &left );
        }

    }; // template <class K, class V, bool KA, bool VA> class Page

}; // namespace BTRee

#endif // BTREE_PAGE_H