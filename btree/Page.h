#ifndef BTREE_PAGE_H
#define BTREE_PAGE_H

#include <new>
#include <stdlib.h>
#include <cstdint>

#include "Types.h"
#include "PageFunctions.h"
#include "PageStreamASCII.h"

// ToDo: Key specific streaming, pass as program argument to constructor
// ToDo: Memory alignment of key, value and size data arrays (performance ?)
// ToDo: Add modifier functions for const Page copying modified content to newly allocated Page

namespace BTree {

    // Template arguments:
    //    class K   Object class of keys
    //    class V   Object class of values
    //    bool KA   Keys are arrays of K
    //    bool VA   Values are arrays of V
    template <class K, class V, bool KA, bool VA>
    class Page {
    public:
        PageHeader header;
        Page() = delete;
        Page( const PageDepth depth ) {
            static const std::string signature( "Page<K,V,false,false>::Page( const PageDepth depth )" );
            if (maxKeySize() < sizeof(K)) throw signature + " - Invalid key size";
            if (maxValueSize() < sizeof(V)) throw signature + " - Invalid value size";
            header.depth = depth;
            header.count = 0;
            initialize<KA,VA>();
        }
        inline bool empty() const { return (header.count == 0); }
        inline PageSize maxKeySize() const { return ((header.capacity - sizeof(PageHeader)) / 8); };
        inline PageSize maxValueSize() const { return ((header.capacity - sizeof(PageHeader)) / 8); };
        friend class PageFunctions;
        friend class ASCIIPageStreamer;

    private:
        // Scalar key to scalar value page layout:
        //   header        : page type information
        //   split         : Split value (if any)
        //   keys          : Scalar key array
        //   values        : Scalar value array
        // Array key to scalar value page layout:
        //   header        : page type information
        //   split         : Split value (if any)
        //   values        : Scalar value array
        //   key indeces   : Cumulative size of variable size array keys, index of key in key data
        //   key values    : Variable size array key values, packed
        // Scalar key to array value page layout:
        //   header        : page type information
        //   split         : Split value (if any)
        //   keys          : Scalar key array
        //   value indeces : Cumulative size of variable size array of values, index of value in value data
        //   value data    : Variable size array value data, packed
        // Array key to array value page layout:
        //   header        : page type information
        //   split         : Split value (if any)
        //   key indeces   : Cumulative size of variable size array keys, index of key in key data
        //   value indeces : Cumulative size of variable size array of values, index of value in value data
        //   key data      : Variable size array key data, packed
        //   value data    : Variable size array value data, packed
        inline uint8_t* content() const {
            return ((reinterpret_cast<uint8_t*>(const_cast<PageHeader *>(&header))) + sizeof(PageHeader));
        }
        template <bool AK = KA>
        inline typename std::enable_if<(!AK), K>::type *keys() const {
            return (reinterpret_cast<K*>(const_cast<uint8_t *>(&content()[header.split * sizeof(V)])));
        }
        template <bool AK = KA, bool AV = VA>
        inline typename std::enable_if<(AK && !AV), K>::type *keys() const {
            return (reinterpret_cast<K*>(&keyIndeces()[header.count]));
        }
        template <bool AK = KA, bool AV = VA>
        inline typename std::enable_if<(AK && AV), K>::type *keys() const {
            return (reinterpret_cast<K*>(&valueIndeces()[header.count]));
        }
        template <bool AK = KA, bool AV = VA>
        inline typename std::enable_if<(AK && !AV), PageSize>::type *keyIndeces() const {
            return (reinterpret_cast<PageSize *>(&values()[header.count]));
        }
        template <bool AK = KA, bool AV = VA>
        inline typename std::enable_if<(AK && AV), PageSize>::type *keyIndeces() const {
            return (reinterpret_cast<PageSize *>(const_cast<uint8_t *>(&content()[header.split * sizeof(V)])));
        }
        template <bool AK = KA>
        inline typename std::enable_if<(AK), PageIndex>::type keyIndex(PageIndex index) const {
            return ((index == 0) ? 0 : keyIndeces()[index - 1]);
        }
        template <bool AK = KA, bool AV = VA>
        inline typename std::enable_if<(!AK && !AV), V>::type *values() const {
            return (reinterpret_cast<V*>(&keys()[header.count]));
        }
        template <bool AK = KA, bool AV = VA>
        inline typename std::enable_if<(AK && !AV), V>::type *values() const {
            return (reinterpret_cast<V*>(const_cast<uint8_t *>(&content()[header.split * sizeof(V)])));
        }
        template <bool AK = KA, bool AV = VA>
        inline typename std::enable_if<(AK && AV), V>::type *values() const {
            return (reinterpret_cast<V*>(const_cast<K*>(&keys()[keyIndex(header.count)])));
        }
        template <bool AK = KA, bool AV = VA>
        inline typename std::enable_if<(!AK && AV), V>::type *values() const {
            return (reinterpret_cast<V*>(&valueIndeces()[header.count]));
        }
        template <bool AK = KA, bool AV = VA>
        inline typename std::enable_if<(!AK && AV), PageSize>::type *valueIndeces() const {
            return (reinterpret_cast<PageSize *>(&keys()[header.count]));
        }
        template <bool AK = KA, bool AV = VA>
        inline typename std::enable_if<(AK && AV), PageSize>::type *valueIndeces() const {
            return (reinterpret_cast<PageSize *>(&keyIndeces()[header.count]));
        }
        template <bool AV = VA>
        inline typename std::enable_if<(AV), PageIndex>::type valueIndex(PageIndex index) const {
            return ((index == 0) ? 0 : valueIndeces()[index - 1]);
        }
        template <bool AV = VA>
        inline typename std::enable_if<(!AV), V>::type &splitValue() const {
            return (*reinterpret_cast<V*>(const_cast<uint8_t *>(content())));
        }
        template <bool AV = VA>
        inline typename std::enable_if<(AV), V>::type *splitValue() const {
            return (reinterpret_cast<V*>(const_cast<uint8_t *>(content())));
        }
        template< bool AK = KA, bool AV = VA >
        inline typename std::enable_if<(!AK&&!AV), void>::type initialize() {
        }
        template< bool AK = KA, bool AV = VA >
        inline typename std::enable_if<(AK&&!AV), void>::type initialize() {
            keyIndeces()[ 0 ] = 0;
        }
        template< bool AK = KA, bool AV = VA >
        inline typename std::enable_if<(!AK&&AV), void>::type initialize() {
            valueIndeces()[ 0 ] = 0;
        }
        template< bool AK = KA, bool AV = VA >
        inline typename std::enable_if<(AK&&AV), void>::type initialize() {
            keyIndeces()[ 0 ] = 0;
            valueIndeces()[ 0 ] = 0;
        }

    public:
        // Return page content usage required to store entries up to the requested index.
        template< bool AK = KA, bool AV = VA >
        inline typename std::enable_if<(!AK&&!AV), PageSize>::type filling( PageIndex index ) const {
            return (sizeof(PageHeader) + (header.split * sizeof(V)) + (index * (sizeof(K) + sizeof(V))));
        }
        template< bool AK = KA, bool AV = VA >
        inline typename std::enable_if<(AK&&!AV), PageSize>::type filling(PageIndex index) const {
            return (sizeof(PageHeader) + (header.split * sizeof(V)) + (index * (sizeof(V) + sizeof(PageIndex))) + (keyIndex(index) * sizeof(K)));
        }
        template< bool AK = KA, bool AV = VA >
        inline typename std::enable_if<(!AK&&AV), PageSize>::type filling(PageIndex index) const {
            return (sizeof(PageHeader) + (header.split * sizeof(V)) + (index * (sizeof(K) + sizeof(PageIndex))) + (valueIndex(index) * sizeof(V)));
        }
        template< bool AK = KA, bool AV = VA >
        inline typename std::enable_if<(AK&&AV), PageSize>::type filling(PageIndex index) const {
            return (sizeof(PageHeader) + (header.split * sizeof(V)) + (2 * index * sizeof(PageIndex)) + (keyIndex(index) * sizeof(K)) + (valueIndex(index) * sizeof(V)));
        }
        // Return number of page bytes in use for entire page content.
        inline PageSize filling() const { return (filling<KA,VA>(header.count)); };

        // Return number of bytes required to store a single key-value entry.
        template <bool AK = KA, bool AV = VA>
        inline typename std::enable_if<(!AK && !AV), PageSize>::type
        entryFilling() const {
            return (sizeof(K) + sizeof(V));
        }
        template <bool AK = KA, bool AV = VA>
        inline typename std::enable_if<(AK && !AV), PageSize>::type
        entryFilling(const PageSize keySize) const {
            return (sizeof(PageSize) + (keySize * sizeof(K)) + sizeof(V));
        }
        template <bool AK = KA, bool AV = VA>
        inline typename std::enable_if<(!AK && AV), PageSize>::type
        entryFilling(const PageSize valueSize) const {
            return (sizeof(K) + sizeof(PageSize) + (valueSize * sizeof(V)));
        }
        template <bool AK = KA, bool AV = VA>
        inline typename std::enable_if<(AK && AV), PageSize>::type
        entryFilling(const PageSize keySize, const PageSize valueSize) const {
            return ((2 * sizeof(PageSize)) + (keySize * sizeof(K)) + (valueSize * sizeof(V)));
        }


        // Determine if an entry will fit in this page.
        template< bool AK = KA, bool AV = VA >
        inline typename std::enable_if<(!AK&&!AV),bool>::type
        entryFit() const {
            if ((filling() + entryFilling()) <= header.capacity) return true;
            return false;
        }
        template< bool AK = KA, bool AV = VA >
        inline typename std::enable_if<(AK&&!AV),bool>::type
        entryFit( const PageSize keySize ) const {
            if ((filling() + entryFilling( keySize )) <= header.capacity) return true;
            return false;
        }
        template< bool AK = KA, bool AV = VA >
        inline typename std::enable_if<(!AK&&AV),bool>::type
        entryFit( const PageSize valueSize ) const {
            if ((filling() + entryFilling( valueSize )) <= header.capacity) return true;
            return false;
        }
        template< bool AK = KA, bool AV = VA >
        inline typename std::enable_if<(AK&&AV),bool>::type
        entryFit( const PageSize keySize, const PageSize valueSize ) const {
            if ((filling() + entryFilling( keySize, valueSize )) <= header.capacity) return true;
            return false;
        }

        // Return number of entries in this page.
        PageIndex size() { return header.count; }

        // Return reference to scalar key at index.
        template <bool AK = KA>
        const typename std::enable_if<(!AK), K>::type &key(PageIndex index) const {
            static const std::string signature( "const K& key( PageIndex index ) const" );
            if (header.count <= index) throw signature + " - Invalid index";
            return (keys()[index]);
        }
        // Return pointer to array key at index.
        template <bool AK = KA>
        const typename std::enable_if<(AK), K>::type *key(PageIndex index) const {
            static const std::string signature( "const K* key( PageIndex index ) const" );
            if (header.count <= index) throw signature + " - Invalid index";
            return (&keys()[keyIndex(index)]);
        }
        // Return size of array key at index.
        template <bool AK = KA>
        typename std::enable_if<(AK), PageSize>::type keySize( PageIndex index ) const {
            static const std::string signature( "PageSize keySize( PageIndex index ) const" );
            if (header.count <= index) throw signature + " - Invalid index";
            if (index == 0) return keyIndeces()[0];
            return (keyIndeces()[index] - keyIndeces()[index - 1]);
        }
        // Return reference to scalar value at index.
        template <bool AV = VA>
        const typename std::enable_if<(!AV), V>::type &value(PageIndex index) const {
            static const std::string signature( "const V& value( PageIndex index ) const" );
            if (header.count <= index) throw signature + " - Invalid index";
             return (values()[index]);
        }
        // Return pointer to array value at index.
        template <bool AV = VA>
        const typename std::enable_if<(AV), V>::type *value( PageIndex index ) const {
            static const std::string signature( "const V* value( PageIndex index ) const" );
            if (header.count <= index) throw signature + " - Invalid index";
            return (&values()[valueIndex(index)]);
        }
        // Return size of array value at index.
        template <bool AV = VA>
        typename std::enable_if<(AV), PageSize>::type valueSize( PageIndex index ) const {
            static const std::string signature( "PageSize valueSize( PageIndex index ) const" );
            if (header.count <= index) throw signature + " - Invalid index";
            if (index == 0) return (valueIndeces()[0]);
            return (valueIndeces()[index] - valueIndeces()[index - 1]);
        }

        // Test if split value is defined for this page
        bool splitDefined() const { return (header.split != 0); }
        // Return the scalar split value (if any)
        template <bool AV = VA>
        const typename std::enable_if<(!AV), V>::type &split() const {
            static const std::string signature( "const V& split() const" );
            if (header.split == 0) throw signature + " - No split defined";
            return splitValue<false>();
        }
        // Return pointer to array split value (if any)
        template <bool AV = VA>
        const typename std::enable_if<(AV), V>::type *split() const {
            static const std::string signature( "const V* split() const" );
            if (header.split == 0) throw signature + " - No split defined";
            return splitValue<true>();
        }
        // Return size of array split value (if any)
        template <bool AV = VA>
        typename std::enable_if<(AV), PageSize>::type splitSize() const {
            static const std::string signature( "PageSize splitSize() const" );
            if (header.split == 0) throw signature + " - No split defined";
            return (header.split);
        }

        // Set scalar split value
        template <bool AV = VA>
        typename std::enable_if<(!AV), void>::type split( const V& value, Page<K,V,KA,false>* dst = nullptr ) {
            static const std::string signature( "void split( const V& value, Page<K,V,KA,false>* dst )" );
            if (header.capacity < (filling() + sizeof(V) - (header.split * sizeof(V)))) throw signature + " - Overflow";
            PageFunctions::pageSplit( *this, ((dst) ? *dst : *this), value );
        }
         // Set array split value
       template <bool AV = VA>
        typename std::enable_if<(AV), void>::type split( const V* value, PageSize valueSize, Page<K,V,KA,true>* dst = nullptr ) {
            static const std::string signature( "void split( const V* value, PageSize valueSize, Page<K,V,KA,true>* dst )" );
            if (header.capacity < (filling() + (valueSize * sizeof(V)) - (header.split * sizeof(V)))) throw signature + " - Overflow";
            PageFunctions::pageSplit( *this, ((dst) ? *dst : *this), value, valueSize );
        }

        // Remove split value in scalar key - scalar value page
        void removeSplit( Page<K,V,KA,VA>* dst = nullptr ) {
            static const std::string signature( "void removeSplit( Page<K,V,KA,VA>* dst )" );
            if (header.split == 0) throw signature + " - No split defined";
            PageFunctions::pageRemoveSplit( *this, ((dst) ? *dst : *this) );
        }

        // Insert an scalar key - scalar value entry at an index
        template <bool AK = KA, bool AV = VA>
        typename std::enable_if<(!AK && !AV), void>::type insert( PageIndex index, const K& key, const V& value, Page<K,V,false,false>* copy = nullptr) {
            static const std::string signature( "void Page<K,V,false,false>::insert( PageIndex index, const K& key, const V& value, Page<K,V,false,false>* copy )" );
            if (header.count < index) throw signature + " - Invalid index";
            if (header.capacity < (filling() + entryFilling())) throw signature + " - Overflow";
            PageFunctions::pageInsert( *this, ((copy) ? *copy : *this), index, key, value );
        }
        // Insert an array key - scalar value entry at an index
        template <bool AK = KA, bool AV = VA>
        typename std::enable_if<(AK && !AV), void>::type insert( PageIndex index, const K* key, const PageSize keySize, const V& value, Page<K,V,true,false>* copy = nullptr) {
            static const std::string signature( "void Page<K,V,KA,VA>::insert( PageIndex index, const K* key, const PageSize keySize, const V& value, Page<K,V,true,false>* copy )" );
            if (header.count < index) throw signature + " - Invalid index";
            if ((keySize == 0) || (maxKeySize() < (keySize * sizeof(K)))) throw signature + " - Invalid key size";
            if (header.capacity < (filling() + entryFilling( keySize ))) throw signature + " - Overflow";
            PageFunctions::pageInsert( *this, ((copy) ? *copy : *this), index, key, keySize, value );
        }
        // Insert a scalar key - array value entry at an index
        template <bool AK = KA, bool AV = VA>
        typename std::enable_if<(!AK && AV), void>::type insert( PageIndex index, const K& key, const V* value, const PageSize valueSize, Page<K,V,false,true>* copy = nullptr) {
            static const std::string signature( "Page<K,V,KA,VA>::insert( PageIndex index, const K& key, const V* value, const PageSize valueSize, Page<K,V,false,true>* copy ) - Invalid index" );
            if (header.count < index) throw signature + " - Invalid index";
            if ((valueSize == 0) || (maxValueSize() < (valueSize * sizeof(V)))) throw signature + " - Invalid value size";
            if (header.capacity < (filling() + entryFilling( valueSize ))) throw signature + " - Overflow";
            PageFunctions::pageInsert( *this, ((copy) ? *copy : *this), index, key, value, valueSize );
        }
        // Insert an array key - array value entry at an index
        template <bool AK = KA, bool AV = VA>
        typename std::enable_if<(AK && AV), void>::type insert( PageIndex index, const K* key, const PageSize keySize, const V* value, const PageSize valueSize, Page<K,V,true,true>* copy = nullptr) {
            static const std::string signature( "void Page<K,V,KA,VA>::insert( PageIndex index, const K* key, const PageSize keySize, const V* value, const PageSize valueSize, Page<K,V,true,true>* copy )" );
            if (header.count < index) throw signature + " - Invalid index";
            if ((keySize == 0) || (maxKeySize() < (keySize * sizeof(K)))) throw signature + " - Invalid key size";
            if ((valueSize == 0) || (maxValueSize() < (valueSize * sizeof(V)))) throw signature + " - Invalid value size";
            if (header.capacity < (filling() + entryFilling( keySize, valueSize ))) throw signature + " - Overflow";
            PageFunctions::pageInsert( *this, ((copy) ? *copy : *this), index, key, keySize, value, valueSize );
        }

        // Replace a value at an index
        // Replace a scalar value
        template <bool AV = VA> 
        typename std::enable_if<(!AV), void>::type replace( PageIndex index, const V& value, Page<K,V,KA,false>* copy = nullptr) {
            static const std::string signature( "void Page<K,V,KA,false>::replace( PageIndex index, const V& value, Page<K,V,KA,false>* copy )" );
            if (header.count < index) throw signature + " - Invalid index";
            PageFunctions::pageReplace( *this, ((copy) ? *copy : *this), index, value );
        }
        // Replace an array value at an index
        template <bool AV = VA>
        typename std::enable_if<(AV), void>::type replace( PageIndex index, const V* value, const PageSize valueSize, Page<K,V,KA,true>* copy = nullptr) {
            static const std::string signature( "void Page<K,V,KA,VA>::replace( PageIndex index, const V* value, const PageSize valueSize, Page<K,V,KA,true>* copy )" );
            if (header.count < index) throw signature + " - Invalid index";
            if (header.capacity < (filling() + (valueSize * sizeof(V)) - (this->valueSize(index) * sizeof(V)))) throw signature + " - Overflow";
            PageFunctions::pageReplace( *this, ((copy) ? *copy : *this), index, value, valueSize );
        }

        // Exchange a key-value entry at an index
        // Exchange a scalar key scalar value entry
        template <bool AK = KA, bool AV = VA>
        typename std::enable_if<(!AK && !AV), void>::type exchange(PageIndex index, const K& key, const V& value, Page<K,V,false,false>* copy = nullptr) {
            static const std::string signature( "void exchange( PageIndex index, const K& key, const V* value, const PageSize valueSize, Page<K,V,false,false>* copy )" );
            if (header.count < index) throw signature + " - Invalid index";
            PageFunctions::pageExchange( *this, ((copy) ? *copy : *this), index, key, value );
        }
        // Exchange an array key scalar value entry
        template <bool AK = KA, bool AV = VA>
        typename std::enable_if<(AK && !AV), void>::type exchange( PageIndex index, const K* key, const PageSize keySize, const V& value, Page<K,V,true,false>* copy = nullptr) {
            static const std::string signature( "void exchange( PageIndex index, const K* key, const PageSize keySize, const V& value, Page<K,V,true,false>* copy )" );
            if (header.count < index) throw signature + " - Invalid index";
            if ((keySize == 0) || (maxKeySize() < (keySize * sizeof(K)))) throw signature + " - Invalid key size";
            if (header.capacity < (filling() + entryFilling( keySize ) - entryFilling( this->keySize(index) ))) throw signature + " - Overflow";
            PageFunctions::pageExchange( *this, ((copy) ? *copy : *this), index, key, keySize, value );
        }
        // Exchange a scalar key array value entry
        template <bool AK = KA, bool AV = VA>
        typename std::enable_if<(!AK && AV), void>::type exchange( PageIndex index, const K& key, const V* value, const PageSize valueSize, Page<K,V,false,true>* copy = nullptr) {
            static const std::string signature( "void exchange( PageIndex index, const K& key, const V* value, const PageSize valueSize, Page<K,V,false,true>* copy )" );
            if (header.count < index) throw signature + " - Invalid index";
            if ((valueSize == 0) || (maxValueSize() < (valueSize * sizeof(V)))) throw signature + " - Invalid value size";
            if (header.capacity < (filling() + entryFilling( valueSize ) - entryFilling( this->valueSize(index) ))) throw signature + " - Overflow";
            PageFunctions::pageExchange( *this, ((copy) ? *copy : *this), index, key, value, valueSize );
        }
        // Exchange an array key array value entry
        template <bool AK = KA, bool AV = VA>
        typename std::enable_if<(AK && AV), void>::type exchange( PageIndex index, const K* key, const PageSize keySize, const V* value, const PageSize valueSize, Page<K,V,true,true>* copy = nullptr) {
            static const std::string signature( "void exchange( PageIndex index, const K* key, const PageSize keySize, const V* value, const PageSize valueSize, Page<K,V,true,true>* copy )" ) ;
            if (header.count < index) throw signature + " - Invalid index";
            if ((keySize == 0) || (maxKeySize() < (keySize * sizeof(K)))) throw signature + " - Invalid key size";
            if ((valueSize == 0) || (maxValueSize() < (valueSize * sizeof(V)))) throw signature + " - Invalid value size";
            if (header.capacity < (filling() + entryFilling( keySize, valueSize ) - entryFilling( this->keySize(index), this->valueSize(index) ))) throw signature + " - Overflow";
            PageFunctions::pageExchange( *this, ((copy) ? *copy : *this), index, key, keySize, value, valueSize );
        }

        // Remove the key - value entry at an index
        void remove( PageIndex index, Page<K,V,KA,VA>* copy = nullptr ) {
            static const std::string signature( "void remove( PageIndex index, Page<K,V,KA,VA>* copy" );
            if (header.count <= index) throw signature + " - Invalid index";
            PageFunctions::pageRemove( *this, ((copy) ? *copy : *this), index );
        }

        // Shift page content from indexed entry up to last entry to lower entries in another page
        void shiftRight( Page &right, PageIndex index, Page* copy = nullptr, Page* copyRight = nullptr ) {
            static const std::string signature( "void shiftRight( Page &right, PageIndex index, Page* copy, Page* copyRight" );
            if (header.count < index) throw signature + " - Invalid index or empty page";
            if (right.header.capacity < (right.filling() + (filling() - filling( index )))) throw signature + " - Overflow";
            PageFunctions::pageShiftRight( *this, right, ((copy) ? *copy : *this), ((copyRight) ? *copyRight : right), index );
        }
        // Shift page content from first entry up to indexed entry up to higher entries in another page
        void shiftLeft( Page &left, PageIndex index, Page* copy = nullptr, Page* copyLeft = nullptr ) {
            static const std::string signature( "void shiftLeft( Page &left, PageIndex index, Page* copy, Page* copyLeft" );
            if (header.count < index) throw signature + " - Invalid index or empty page";
            if (left.header.capacity < (left.filling() + filling( index ))) throw signature + " - Overflow";
            PageFunctions::pageShiftLeft( *this, left, ((copy) ? *copy : *this), ((copyLeft) ? *copyLeft : left), index );
        }

    }; // template <class K, class V, bool KA, bool VA> class Page

}; // namespace BTRee

#endif // BTREE_PAGE_H