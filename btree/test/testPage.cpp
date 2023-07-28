#include "Page.h"
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <random>
#include <algorithm>

using namespace std;
using namespace BTree;

// ToDo: Test all exceptions thrown by Page (invalid index, invalid key size, invalid value size, overflow, no split defined)

//const int PageCapacity = 4096;
const int PageCapacity = 8192;
const int MinArray = 2; // Value must be greater than 1 as vector of length 1 is assumed to be a scalar
const int MaxArray = 23;

mt19937 gen32;
mt19937 gen64;

template< class K, class V, bool AK, bool AV >
Page<K,V,AK,AV>* allocatePage() {
    void* page = malloc( PageCapacity );
    PageHeader* header = reinterpret_cast< PageHeader* >( page );
    header->page = PageLink( 47 );
    header->capacity = PageCapacity;
    header->free = 0;
    header->modified = 0;
    header->persistent = 0;
    header->recover = 0;
    header->stored = 0;
    header->depth = 0;
    header->count = 0;
    header->split = 0;
    return( new( page ) Page<K,V,AK,AV>( 0 ) );
}

template< class T >
T generateValue() { return( gen64() % (uint64_t(1) << (sizeof( T ) * 8) ) ); }

template< class T >
vector<T> generate( uint32_t min, uint32_t max ) {
    vector<T> value;
    uint32_t n = 1;
    value.push_back( generateValue<T>() );
    while (n < min) { value.push_back( generateValue<T>() ); ++n; }
    if (min < max) {
        uint32_t N = min + (gen32() % (max - min));
        while (n < N) { value.push_back( generateValue<T>() ); ++n; }
    }
    return value;
}

template< class T >
void logValue( ostream& log, const vector<T>& value ) {
    log << "[ ";
    size_t N = value.size();
    if (0 < N) {
        for (int i = 0; i < (N - 1); ++i)  log << value[ i ] << ", ";
        log << value[ N - 1 ];
    }
    log << " ]";
}

template< class K, class V >
class PageContent {
public:
    vector<V> splitValue;
    vector<vector<K>> keys;
    vector<vector<V>> values;
    PageContent() {}
    PageSize size() const { return keys.size(); }
    void clear() {
        splitValue.clear();
        keys.clear();
        values.clear();
    }
    PageSize entryFilling( const vector<K>& key, const vector<V>& value ) const {
        PageSize fill = (key.size() * sizeof( K )) + (value.size() * sizeof( V ));
        if (1 < key.size()) fill += sizeof( PageIndex );
        if (1 < value.size()) fill += sizeof( PageIndex );
        return fill;
    }
    void assign( const PageContent<K,V>& content ) {
        splitValue = content.splitValue;
        keys = content.keys;
        values = content.values;
    }
    void split( const vector<V>& value ) { splitValue = value; }
    const vector<V>& split() const { return splitValue; }
    bool splitDefined() const { return( 0 < splitValue.size() ); }
    void removeSplit() { splitValue.clear(); }
    void insert( PageIndex index, vector<K>& key, vector<V>& value ) {
        keys.insert( (keys.begin() + index), key );
        values.insert( (values.begin() + index), value );
    }
    void replace( PageIndex index, vector<V>& value ) {
        values[ index ] = value;
    }
    void replace( PageIndex index, vector<K>& key, vector<V>& value ) {
        keys[ index ] = key;
        values[ index ] = value;
    }
    void remove( PageIndex index ) {
        keys.erase( keys.begin() + index );
        values.erase( values.begin() + index );
    }
    void shiftRight( PageIndex index, PageContent<K,V>& content ) {
        content.keys.insert( content.keys.begin(), (keys.begin() + index), keys.end() );
        content.values.insert( content.values.begin(), (values.begin() + index), values.end() );
        keys.erase( (keys.begin() + index), keys.end() );
        values.erase( (values.begin() + index), values.end() );
    }
    void shiftLeft( PageIndex index, PageContent<K,V>& content ) {
        content.keys.insert( content.keys.end(), keys.begin(), (keys.begin() + index) );
        content.values.insert( content.values.end(), values.begin(), (values.begin() + index) );
        keys.erase( keys.begin(), (keys.begin() + index) );
        values.erase( values.begin(), (values.begin() + index) );
    }
};

template< class K, class V, bool AK, bool AV, std::enable_if_t<(!AK&&!AV),bool> = true >
PageSize pageEntryFit( const Page<K,V,false,false>& page, const vector<K>& key, const vector<V>& value ) {
    return page.entryFit();
}
template< class K, class V, bool AK, bool AV, std::enable_if_t<(AK&&!AV),bool> = true >
PageSize pageEntryFit( const Page<K,V,true,false>& page, const vector<K>& key, const vector<V>& value ) {
    return page.entryFit( key.size() );
}
template< class K, class V, bool AK, bool AV, std::enable_if_t<(!AK&&AV),bool> = true >
PageSize pageEntryFit( const Page<K,V,false,true>& page, const vector<K>& key, const vector<V>& value ) {
    return page.entryFit( value.size() );
}
template< class K, class V, bool AK, bool AV, std::enable_if_t<(AK&&AV),bool> = true >
PageSize pageEntryFit( const Page<K,V,true,true>& page, const vector<K>& key, const vector<V>& value ) {
    return page.entryFit( key.size(), value.size() );
}
template< class K, class V, bool AK, bool AV, std::enable_if_t<(!AK&&!AV),bool> = true >
PageSize pageEntryFilling( const Page<K,V,false,false>& page, const vector<K>& key, const vector<V>& value ) {
    return page.entryFilling();
}
template< class K, class V, bool AK, bool AV, std::enable_if_t<(AK&&!AV),bool> = true >
PageSize pageEntryFilling( const Page<K,V,true,false>& page, const vector<K>& key, const vector<V>& value ) {
    return page.entryFilling( key.size() );
}
template< class K, class V, bool AK, bool AV, std::enable_if_t<(!AK&&AV),bool> = true >
PageSize pageEntryFilling( const Page<K,V,false,true>& page, const vector<K>& key, const vector<V>& value ) {
    return page.entryFilling( value.size() );
}
template< class K, class V, bool AK, bool AV, std::enable_if_t<(AK&&AV),bool> = true >
PageSize pageEntryFilling( const Page<K,V,true,true>& page, const vector<K>& key, const vector<V>& value ) {
    return page.entryFilling( key.size(), value.size() );
}
template< class K, class V, bool AK, bool AV, std::enable_if_t<(!AV),bool> = true >
void pageSplit( Page<K,V,AK,false>& page, const vector<V>& value ) {
    page.split( value[ 0 ] );
}
template< class K, class V, bool AK, bool AV, std::enable_if_t<(AV),bool> = true >
void pageSplit( Page<K,V,AK,true>& page, const vector<V>& value ) {
    page.split( value.data(), value.size() );
}
template< class K, class V, bool AK, bool AV, std::enable_if_t<(!AV),bool> = true >
vector<V> pageSplit( const Page<K,V,AK,false>& page ) {
    return vector<V>( 1, page.split() );
}
template< class K, class V, bool AK, bool AV, std::enable_if_t<(AV),bool> = true >
vector<V> pageSplit( const Page<K,V,AK,true>& page ) {
    const V* value = page.split();
    PageSize size = page.splitSize();
    vector<V> result;
    for (PageSize i = 0; i < size; ++i) result.push_back( *(value + i) );
    return result;
}
template< class K, class V, bool AK, bool AV, std::enable_if_t<(!AK),bool> = true >
vector<K> pageKey( const Page<K,V,false,AV>& page, PageIndex index ) {
    return vector<K>( 1, page.key( index) );
}
template< class K, class V, bool AK, bool AV, std::enable_if_t<(AK),bool> = true >
vector<K> pageKey( const Page<K,V,true,AV>& page, PageIndex index ) {
    const K* value = page.key( index );
    PageSize size = page.keySize( index );
    vector<K> result;
    for (PageSize i = 0; i < size; ++i) result.push_back( *(value + i) );
    return result;
}
template< class K, class V, bool AK, bool AV, std::enable_if_t<(!AV),bool> = true >
vector<V> pageValue( const Page<K,V,AK,false>& page, PageIndex index ) {
    return vector<V>( 1, page.value( index) );
}
template< class K, class V, bool AK, bool AV, std::enable_if_t<(AV),bool> = true >
vector<V> pageValue( const Page<K,V,AK,true>& page, PageIndex index ) {
    const V* value = page.value( index );
    PageSize size = page.valueSize( index );
    vector<V> result;
    for (PageSize i = 0; i < size; ++i) result.push_back( *(value + i) );
    return result;
}
template< class K, class V, bool AK, bool AV, std::enable_if_t<(!AK&&!AV),bool> = true >
void pageInsert( Page<K,V,false,false>& page, PageIndex index, const vector<K>& key, const vector<V>& value ) {
    page.insert( index, key[ 0 ], value[ 0 ] );
}
template< class K, class V, bool AK, bool AV, std::enable_if_t<(AK&&!AV),bool> = true >
void pageInsert( Page<K,V,true,false>& page, PageIndex index, const vector<K>& key, const vector<V>& value ) {
    page.insert( index, key.data(), key.size(), value[ 0 ] );
}
template< class K, class V, bool AK, bool AV, std::enable_if_t<(!AK&&AV),bool> = true >
void pageInsert( Page<K,V,false,true>& page, PageIndex index, const vector<K>& key, const vector<V>& value ) {
    page.insert( index, key[ 0 ], value.data(), value.size() );
}
template< class K, class V, bool AK, bool AV, std::enable_if_t<(AK&&AV),bool> = true >
void pageInsert( Page<K,V,true,true>& page, PageIndex index, const vector<K>& key, const vector<V>& value ) {
    page.insert( index, key.data(), key.size(), value.data(), value.size() );
}
template< class K, class V, bool AK, bool AV, std::enable_if_t<(!AK&&!AV),bool> = true >
void pageInsert( const Page<K,V,false,false>& page, PageIndex index, const vector<K>& key, const vector<V>& value, Page<K,V,false,false>& copy ) {
    page.insert( index, key[ 0 ], value[ 0 ], &copy );
}
template< class K, class V, bool AK, bool AV, std::enable_if_t<(AK&&!AV),bool> = true >
void pageInsert( const Page<K,V,true,false>& page, PageIndex index, const vector<K>& key, const vector<V>& value, Page<K,V,true,false>& copy ) {
    page.insert( index, key.data(), key.size(), value[ 0 ], &copy );
}
template< class K, class V, bool AK, bool AV, std::enable_if_t<(!AK&&AV),bool> = true >
void pageInsert( const Page<K,V,false,true>& page, PageIndex index, const vector<K>& key, const vector<V>& value, Page<K,V,false,true>& copy ) {
    page.insert( index, key[ 0 ], value.data(), value.size(), &copy );
}
template< class K, class V, bool AK, bool AV, std::enable_if_t<(AK&&AV),bool> = true >
void pageInsert( const Page<K,V,true,true>& page, PageIndex index, const vector<K>& key, const vector<V>& value, Page<K,V,true,true>& copy ) {
    page.insert( index, key.data(), key.size(), value.data(), value.size(), &copy );
}
template< class K, class V, bool AK, bool AV, std::enable_if_t<(!AV),bool> = true >
void pageReplace( Page<K,V,AK,false>& page, PageIndex index, const vector<V>& value ) {
    page.replace( index, value[ 0 ] );
}
template< class K, class V, bool AK, bool AV, std::enable_if_t<(AV),bool> = true >
void pageReplace( Page<K,V,AK,true>& page, PageIndex index, const vector<V>& value ) {
    page.replace( index, value.data(), value.size() );
}
template< class K, class V, bool AK, bool AV, std::enable_if_t<(!AV),bool> = true >
void pageReplace( const Page<K,V,AK,false>& page, PageIndex index, const vector<V>& value, Page<K,V,AK,false>& copy ) {
    page.replace( index, value[ 0 ], &copy );
}
template< class K, class V, bool AK, bool AV, std::enable_if_t<(AV),bool> = true >
void pageReplace( const Page<K,V,AK,true>& page, PageIndex index, const vector<V>& value, Page<K,V,AK,true>& copy ) {
    page.replace( index, value.data(), value.size(), &copy );
}
template< class K, class V, bool AK, bool AV, std::enable_if_t<(!AK&&!AV),bool> = true >
void pageReplace( Page<K,V,false,false>& page, PageIndex index, const vector<K>& key, const vector<V>& value ) {
    page.replace( index, key[ 0 ], value[ 0 ] );
}
template< class K, class V, bool AK, bool AV, std::enable_if_t<(AK&&!AV),bool> = true >
void pageReplace( Page<K,V,true,false>& page, PageIndex index, const vector<K>& key, const vector<V>& value ) {
    page.replace( index, key.data(), key.size(), value[ 0 ] );
}
template< class K, class V, bool AK, bool AV, std::enable_if_t<(!AK&&AV),bool> = true >
void pageReplace( Page<K,V,false,true>& page, PageIndex index, const vector<K>& key, const vector<V>& value ) {
    page.replace( index, key[ 0 ], value.data(), value.size() );
}
template< class K, class V, bool AK, bool AV, std::enable_if_t<(AK&&AV),bool> = true >
void pageReplace( Page<K,V,true,true>& page, PageIndex index, const vector<K>& key, const vector<V>& value ) {
    page.replace( index, key.data(), key.size(), value.data(), value.size() );
}
template< class K, class V, bool AK, bool AV, std::enable_if_t<(!AK&&!AV),bool> = true >
void pageReplace( const Page<K,V,false,false>& page, PageIndex index, const vector<K>& key, const vector<V>& value, Page<K,V,false,false>& copy ) {
    page.replace( index, key[ 0 ], value[ 0 ], &copy );
}
template< class K, class V, bool AK, bool AV, std::enable_if_t<(AK&&!AV),bool> = true >
void pageReplace( const Page<K,V,true,false>& page, PageIndex index, const vector<K>& key, const vector<V>& value, Page<K,V,true,false>& copy ) {
    page.replace( index, key.data(), key.size(), value[ 0 ], &copy );
}
template< class K, class V, bool AK, bool AV, std::enable_if_t<(!AK&&AV),bool> = true >
void pageReplace( const Page<K,V,false,true>& page, PageIndex index, const vector<K>& key, const vector<V>& value, Page<K,V,false,true>& copy ) {
    page.replace( index, key[ 0 ], value.data(), value.size(), &copy );
}
template< class K, class V, bool AK, bool AV, std::enable_if_t<(AK&&AV),bool> = true >
void pageReplace( const Page<K,V,true,true>& page, PageIndex index, const vector<K>& key, const vector<V>& value, Page<K,V,true,true>& copy ) {
    page.replace( index, key.data(), key.size(), value.data(), value.size(), &copy );
}

template< class K, class V, bool AK, bool AV >
class PageTester {
private:
    ostream& log;
    vector<K> generateKey() const { return generate<K>( (AK ? MinArray : 1), (AK ? MaxArray : 1) ); }
    vector<V> generateValue() const { return generate<V>( (AV ? MinArray : 1), (AV ? MaxArray : 1) ); }
    vector<K> generateMaxKey() const { return generate<K>( (AK ? MaxArray : 1), (AK ? MaxArray : 1) ); }
    vector<V> generateMaxValue() const { return generate<V>( (AV ? MaxArray : 1), (AV ? MaxArray : 1) ); }
    uint32_t validateContent( const Page<K,V,AK,AV>& page, const class PageContent<K,V>& content ) const {
        uint32_t errors = 0;
        if (content.splitDefined() != page.splitDefined()) {
            log << "Split defined error  : Expected " << content.splitDefined() << ", actual " << page.splitDefined() << "!\n";
            errors += 1;
        } else if (content.splitDefined() && ( content.split() != pageSplit<K,V,AK,AV>( page ))) {
            log << "Split value error : Expected "; logValue( log, content.split() ); log << ", actual "; logValue( log, pageSplit<K,V,AK,AV>( page ) ); log << "!\n";
            errors += 1;
        }
        if (page.size() != content.size()) {
            log << "Page size error : Expected " << content.size() << ", actual " << page.size() << "!\n";
            errors += 1;
        }
        for (PageIndex index = 0; index < content.keys.size(); ++index) {
            if (content.keys[ index ] != pageKey<K,V,AK,AV>( page, index )) {
                log << "Key error at " << index << " : Expected "; logValue( log, content.keys[ index ] ); log << ", actual "; logValue( log, pageKey<K,V,AK,AV>( page, index ) ); log << "!\n";
                errors += 1;
            }
            if (content.values[ index ] != pageValue<K,V,AK,AV>( page, index )) {
                log << "Value error at " << index << " : Expected "; logValue( log, content.values[ index ] ); log << ", actual "; logValue( log, pageValue<K,V,AK,AV>( page, index ) ); log << "!\n";
                errors += 1;
            }
        }
        PageSize filling = sizeof( PageHeader );
        filling += (content.split().size() * sizeof( V ));
        for (PageIndex index = 0; index < content.keys.size(); ++index) {
            filling += content.entryFilling( content.keys[ index ], content.values[ index ] );
        }
        if (page.filling() != filling) {
            log << "Filling error : Expected " << filling << ", actual " << page.filling() << "!\n";
            errors += 1;
        }
        return errors;
    }
    // Fill page (and content) with random key-value entries (approximately) up-to the requested filling.
    void fillPage( Page<K,V,AK,AV>& page, PageSize filling, class PageContent<K,V>& content ) const {
        page.clear();
        content.clear();
        vector<V> splitValue = generateValue();
        pageSplit<K,V,AK,AV>( page, splitValue );
        content.split( splitValue );
        PageIndex index = 0;
        auto key = generateKey();
        auto value = generateValue();
        while ((page.filling() + pageEntryFilling<K,V,AK,AV>( page, key, value )) < filling ) {
            pageInsert<K,V,AK,AV>( page, index, key, value );
            content.insert( index, key, value );
            key = generateKey();
            value = generateValue();
            index += 1;
        }
    }
public:
    PageTester() = delete;
    PageTester( ofstream& logStream ) : log( logStream ) {}
    uint32_t filling() {
        uint32_t errors = 0;
        log << "Filling tests ...\n";
        auto page = allocatePage<K,V,AK,AV>();
        class PageContent<K,V> content;
        auto key = generateMaxKey();
        auto value = generateMaxValue();
        if (!pageEntryFit<K,V,AK,AV>( *page, key, value )) {
            log << "Entry fit error : Expected true, actual false!\n";
            errors += 1;
        }
        fillPage( *page, PageCapacity, content );
        errors += validateContent( *page, content );
        if (pageEntryFilling<K,V,AK,AV>( *page, key, value ) != content.entryFilling( key, value )) {
            log << "Entry filling error : Expected " << content.entryFilling( key, value ) << ", actual " << pageEntryFilling<K,V,AK,AV>( *page, key, value ) << "!\n";
            errors += 1;
        }
        if (pageEntryFit<K,V,AK,AV>( *page, key, value )) {
            log << "Entry fit error : Expected false, actual true!\n";
            errors += 1;
        }
        PageSize indexedFill = 0;
        for (int index = 0; index < content.size(); ++index) {
            if (page->indexedFilling( index ) != indexedFill) {
                log << "Indexed filling error : Expected " << indexedFill << ", actual " << page->indexedFilling( index ) << "!\n";
                errors += 1;
            }
            indexedFill += content.entryFilling( content.keys[ index ], content.values[ index ] );
        }
        delete page;
        return errors;
    }
    uint32_t split() {
        uint32_t errors = 0;
        log << "Split tests ...\n";
        auto page = allocatePage<K,V,AK,AV>();
        class PageContent<K,V> content;
        fillPage( *page, ((3 * PageCapacity) / 4), content );
        errors += validateContent( *page, content );
        page->removeSplit();
        content.removeSplit();
        errors += validateContent( *page, content );
        vector<V> splitValue = generateValue();
        pageSplit<K,V,AK,AV>( *page, splitValue );
        content.split( splitValue );
        errors += validateContent( *page, content );
        delete page;
        return errors;
    }
    uint32_t insert() {
        uint32_t errors = 0;
        {
            log << "Insert tests (in-place)...\n";
            auto page = allocatePage<K,V,AK,AV>();
            class PageContent<K,V> content;
            errors += validateContent( *page, content );
            auto key = generateKey();
            auto value = generateValue();
            while ((page->filling() + pageEntryFilling<K,V,AK,AV>( *page, key, value )) < PageCapacity ) {
                PageIndex index = (gen32() % (content.size() + 1));
                pageInsert<K,V,AK,AV>( *page, index, key, value );
                content.insert( index, key, value );
                errors += validateContent( *page, content );
                key = generateKey();
                value = generateValue();
            }
            log << "Inserted " << content.size() << " key-value entries at random positions.\n";
            log << "Page filling " << ((float(page->filling()) / PageCapacity) * 100) << " %.\n";
            delete page;
        }
        {
            log << "Insert tests (copy-on-update)...\n";
            auto page = allocatePage<K,V,AK,AV>();
            auto copy = allocatePage<K,V,AK,AV>();
            class PageContent<K,V> content;
            class PageContent<K,V> copyContent;
            fillPage( *page, (PageCapacity / 2), content );
            errors += validateContent( *page, content );
            errors += validateContent( *copy, copyContent );
            for (PageIndex index = 0; index <= content.size(); ++index) {
                auto key = generateKey();
                auto value = generateValue();
                pageInsert<K,V,AK,AV>( *page, index, key, value, *copy );
                copyContent.assign( content );
                copyContent.insert( index, key, value );
                errors += validateContent( *page, content );
                errors += validateContent( *copy, copyContent );
                key = generateKey();
                value = generateValue();
            }
            log << "Inserted " << (content.size() + 1) << " key-value entries at all positions.\n";
            delete page;
            delete copy;
        }
        return errors;
    }
    uint32_t replace() {
        uint32_t errors = 0;
        {
            log << "Replace value tests (in-place)...\n";
            auto page = allocatePage<K,V,AK,AV>();
            class PageContent<K,V> content;
            fillPage( *page, (PageCapacity / 2), content );
            errors += validateContent( *page, content );
            int replaceCount = 0;
            for (int i = 0; i < content.size(); ++i) {
                PageIndex index = (gen32() % content.size());
                auto value = generateValue();
                pageReplace<K,V,AK,AV>( *page, index, value );
                content.replace( index, value );
                errors += validateContent( *page, content );
                replaceCount += 1;
            }
            log << "Replaced " << replaceCount << " values at random positions.\n";
            log << "Replace key-value tests (in-place)...\n";
            replaceCount = 0;
            for (int i = 0; i < content.size(); ++i) {
                PageIndex index = (gen32() % content.size());
                auto key = generateKey();
                auto value = generateValue();
                pageReplace<K,V,AK,AV>( *page, index, key, value );
                content.replace( index, key, value );
                errors += validateContent( *page, content );
                replaceCount += 1;
            }
            log << "Replaced " << replaceCount << " key-value entries at random positions.\n";
            delete page;
        }
        {
            log << "Replace value tests (copy-on-update)...\n";
            auto page = allocatePage<K,V,AK,AV>();
            auto copy = allocatePage<K,V,AK,AV>();
            class PageContent<K,V> content;
            class PageContent<K,V> copyContent;
            fillPage( *page, (PageCapacity / 2), content );
            errors += validateContent( *page, content );
            errors += validateContent( *copy, copyContent );
            int replaceCount = 0;
            for (PageIndex index = 0; index < content.size(); ++index) {
                auto value = generateValue();
                pageReplace<K,V,AK,AV>( *page, index, value, *copy );
                copyContent.assign( content );
                copyContent.replace( index, value );
                errors += validateContent( *page, content );
                errors += validateContent( *copy, copyContent );
                replaceCount += 1;
            }
            log << "Replaced " << replaceCount << " values at all positions.\n";
            log << "Replace key-value tests (copy-on-update)...\n";
            replaceCount = 0;
            for (PageIndex index = 0; index < content.size(); ++index) {
                auto key = generateKey();
                auto value = generateValue();
                pageReplace<K,V,AK,AV>( *page, index, key, value, *copy );
                copyContent.assign( content );
                copyContent.replace( index, key, value );
                errors += validateContent( *page, content );
                errors += validateContent( *copy, copyContent );
                replaceCount += 1;
            }
            log << "Replaced " << replaceCount << " key-value entries at all positions.\n";
            delete page;
        }
        return errors;
    }
    uint32_t remove() {
        uint32_t errors = 0;
        {
            log << "Remove tests (in-place)...\n";
            auto page = allocatePage<K,V,AK,AV>();
            class PageContent<K,V> content;
            fillPage( *page, PageCapacity, content );
            errors += validateContent( *page, content );
            int removeCount = 0;
            while (0 < content.size()) {
                PageIndex index = (gen32() % content.size());
                page->remove( index );
                content.remove( index );
                errors += validateContent( *page, content );
                removeCount += 1;
            }
            log << "Removed all " << removeCount << " key-value entries at random positions.\n";
            delete page;
        }
        {
            log << "Remove tests (copy-on-update)...\n";
            auto page = allocatePage<K,V,AK,AV>();
            auto copy = allocatePage<K,V,AK,AV>();
            class PageContent<K,V> content;
            class PageContent<K,V> copyContent;
            fillPage( *page, PageCapacity, content );
            errors += validateContent( *page, content );
            errors += validateContent( *copy, copyContent );
            int removeCount = 0;
            for (PageSize index = 0; index < content.size(); ++index) {
                page->remove( index, copy );
                copyContent.assign( content );
                copyContent.remove( index );
                errors += validateContent( *page, content );
                errors += validateContent( *copy, copyContent );
                removeCount += 1;
            }
            log << "Removed " << removeCount << " key-value entries at all positions.\n";
            delete page;
        }
        return errors;
    }
    uint32_t shiftRight() {
        uint32_t errors = 0;
        {
            log << "Shift right tests (in-place)...\n";
            PageIndex index = 0;
            bool done = false;
            while(!done) {
                auto page = allocatePage<K,V,AK,AV>();
                class PageContent<K,V> content;
                fillPage( *page, (PageCapacity / 4), content );
                errors += validateContent( *page, content );
                if (index < content.size()) {
                    auto rightPage = allocatePage<K,V,AK,AV>();
                    class PageContent<K,V> rightContent;
                    fillPage( *rightPage, (PageCapacity / 4), rightContent );
                    errors += validateContent( *rightPage, rightContent );
                    page->shiftRight( *rightPage, index );
                    content.shiftRight( index, rightContent );
                    errors += validateContent( *page, content );
                    errors += validateContent( *rightPage, rightContent );
                    delete rightPage;
                    index += 1;
                } else {
                    done = true;
                }
                delete page;
            }
            log << "Shifted right from index 0 up to " << index << ".\n";
        }   
        {
            log << "Shift right tests (copy-on-update)...\n";
            PageIndex index = 0;
            bool done = false;
            while(!done) {
                auto page = allocatePage<K,V,AK,AV>();
                auto copy = allocatePage<K,V,AK,AV>();
                class PageContent<K,V> content;
                class PageContent<K,V> copyContent;
                fillPage( *page, (PageCapacity / 4), content );
                errors += validateContent( *page, content );
                errors += validateContent( *copy, copyContent );
                if (index < content.size()) {
                    auto rightPage = allocatePage<K,V,AK,AV>();
                    auto copyRightPage = allocatePage<K,V,AK,AV>();
                    class PageContent<K,V> rightContent;
                    class PageContent<K,V> copyRightContent;
                    fillPage( *rightPage, (PageCapacity / 4), rightContent );
                    errors += validateContent( *rightPage, rightContent );
                    errors += validateContent( *copyRightPage, copyRightContent );
                    page->shiftRight( *rightPage, index, copy, copyRightPage );
                    copyContent.assign( content );
                    copyRightContent.assign( rightContent );
                    copyContent.shiftRight( index, copyRightContent );
                    errors += validateContent( *page, content );
                    errors += validateContent( *copy, copyContent );
                    errors += validateContent( *rightPage, rightContent );
                    errors += validateContent( *copyRightPage, copyRightContent );
                    delete rightPage;
                    index += 1;
                } else {
                    done = true;
                }
                delete page;
            }
            log << "Shifted right from index 0 up to " << index << ".\n";
        }   
        return errors;
    }
    uint32_t shiftLeft() {
        uint32_t errors = 0;
        {
            log << "Shift left tests (in-place)...\n";
            PageIndex index = 0;
            bool done = false;
            while(!done) {
                auto page = allocatePage<K,V,AK,AV>();
                class PageContent<K,V> content;
                fillPage( *page, (PageCapacity / 4), content );
                errors += validateContent( *page, content );
                if (index < content.size()) {
                    auto leftPage = allocatePage<K,V,AK,AV>();
                    class PageContent<K,V> leftContent;
                    fillPage( *leftPage, (PageCapacity / 4), leftContent );
                    errors += validateContent( *leftPage, leftContent );
                    page->shiftLeft( *leftPage, index );
                    content.shiftLeft( index, leftContent );
                    errors += validateContent( *page, content );
                    errors += validateContent( *leftPage, leftContent );
                    delete leftPage;
                    index += 1;
                } else {
                    done = true;
                }
                delete page;
            }
            log << "Shifted left from index 0 up to " << index << ".\n";
        }
        {
            log << "Shift left tests (copy-on-update)...\n";
            PageIndex index = 0;
            bool done = false;
            while(!done) {
                auto page = allocatePage<K,V,AK,AV>();
                auto copy = allocatePage<K,V,AK,AV>();
                class PageContent<K,V> content;
                class PageContent<K,V> copyContent;
                fillPage( *page, (PageCapacity / 4), content );
                errors += validateContent( *page, content );
                errors += validateContent( *copy, copyContent );
                if (index < content.size()) {
                    auto leftPage = allocatePage<K,V,AK,AV>();
                    auto copyLeftPage = allocatePage<K,V,AK,AV>();
                    class PageContent<K,V> leftContent;
                    class PageContent<K,V> copyLeftContent;
                    fillPage( *leftPage, (PageCapacity / 4), leftContent );
                    errors += validateContent( *leftPage, leftContent );
                    errors += validateContent( *copyLeftPage, copyLeftContent );
                    page->shiftLeft( *leftPage, index, copy, copyLeftPage );
                    copyContent.assign( content );
                    copyLeftContent.assign( leftContent );
                    copyContent.shiftLeft( index, copyLeftContent );
                    errors += validateContent( *page, content );
                    errors += validateContent( *copy, copyContent );
                    errors += validateContent( *leftPage, leftContent );
                    errors += validateContent( *copyLeftPage, copyLeftContent );
                    delete leftPage;
                    index += 1;
                } else {
                    done = true;
                }
                delete page;
            }
            log << "Shifted left from index 0 up to " << index << ".\n";
        }
        return errors;
    }
};

template< class K, class V, bool AK, bool AV >
uint32_t doTest( ofstream& log ) {
    PageTester<K,V,AK,AV> tester( log );
    uint32_t errors = 0;
    try {
        errors += tester.filling();
        errors += tester.split();
        errors += tester.insert();
        errors += tester.replace();
        errors += tester.remove();
        errors += tester.shiftRight();
        errors += tester.shiftLeft();
    }
    catch ( string message ) {
        log << "Exception : " << message << "!\n";
        errors += 1;
    }
    catch (...) {
        log << "Exception (...)!\n";
        errors += 1;
    }
    log.flush();
    return errors;
}

int main(int argc, char* argv[]) {
    system( "RMDIR /S /Q testPage" );
    system( "MKDIR testPage" );
    ofstream log;
    log.open( "testPage\\logPage.txt" );
    uint32_t errorCount = 0;
    log << "32-bit unsigned integer key to 16-bit unsigned integer Page...\n" << flush;
    {
        errorCount += doTest<uint32_t,uint16_t,false, false>( log );
    }
    log << "\n8-bit unsigned integer array key to 16-bit unsigned integer Page.\n" << flush;
    {
        errorCount += doTest<uint8_t,uint16_t,true,false>( log );
    }
    log << "\n32-bit unsigned integer key to 8-bit unsigned integer array Page.\n" << flush;
    {
        errorCount += doTest<uint32_t,uint8_t,false,true>( log );
    }
    log << "\n16-bit unsigned integer array key to 16-bit unsigned integer array Page.\n" << flush;
    {
        errorCount += doTest<uint16_t,uint16_t,true,true>( log );
    }
    log << "\n";
    if (0 < errorCount) {
        log << "Total of " << errorCount << " errors detected!\n";
    } else {
        log << "No errors detected.\n";
    }
    log.close();
    exit( errorCount );
};

