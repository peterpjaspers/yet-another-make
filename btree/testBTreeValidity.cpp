#include "BTree.h"
#include "PersistentPagePool.h"
#include <map>
#include <set>
#include <iostream>
#include <fstream>
#include <random>
#include <algorithm>
//#include <boost/filesystem.hpp>

using namespace BTree;
using namespace std;
//using namespace boost::filesystem;

// Test program to measure BTree validity.
// BTree validity is performed in a comprehensive set of use cases.
//
// An administration of which entries are in the BTree is maintained in an std::map data structure.
// This enables validating which entries should be present in the BTree and which should not.
// And additional std::map is required for validation of transaction behavior.
//

// ToDo: Check return value of insert and replace and log appropriately if false.

const int BTreePageSize = 256;
const int MinArray = 2;
const int MaxArray = 15;

mt19937 gen32;

inline uint16_t generateUint16() {
    return( 1 + (gen32() % 10000) );
}
inline uint32_t generateUint32() {
    return( 1 + (gen32() % 1000000000) );
}
void generateUint16Array( vector<uint16_t>& value ) {
    value.clear();
    int n = (MinArray  + (gen32() % (MaxArray - MinArray)));
    for (int i = 0; i < n; i++) value.push_back( generateUint16() );
}

void logUint16Array( ostream& log, const uint16_t* value, PageIndex valueSize ) {
    log << "[ ";
    if (0 < valueSize) {
        for (int i = 0; i < (valueSize - 1); i++)  log << value[ i ] << ", ";
        log << value[ valueSize - 1 ];
    }
    log << " ]";
}

struct Uint16ArrayCompare {
    bool operator()( const vector<uint16_t>* lhs, const vector<uint16_t>* rhs ) const {
        if (compare( lhs->data(), PageIndex(lhs->size()), rhs->data(), PageIndex(rhs->size()) ) < 0) return( true );
        return( false );
    }
    static int compare( const uint16_t* lhs, PageIndex ln, const uint16_t* rhs, PageIndex rn ) {
        PageIndex n = ((ln < rn) ? ln : rn);
        for (int i = 0; i < n; i++) if (lhs[ i ] < rhs[ i ]) return( -1 ); else if (rhs[ i ] < lhs[ i ]) return( +1 );
        if (rn < ln) return( -1 );
        if (ln < rn) return( +1 );
        return( 0 );
    }
};

size_t generateIndex( size_t range ) {
    return( gen32() % range );
}

// ToDo: use boost filesystem in file access of PagePool
PagePool* createPagePool( bool persistent, const string& path, PageSize pageSize ) {
    if ( persistent ) {
        // Determine stored page size (if any)...
        PageSize storedPageSize = PersistentPagePool::pageCapacity( path );
        return new PersistentPagePool( ((0 < storedPageSize) ? storedPageSize : pageSize), path );
    } else {
        return new PagePool( pageSize );
    }
}
void destroyPagePool( bool persistent, PagePool* pool ) {
    if (persistent) {
        delete( dynamic_cast<PersistentPagePool*>(pool) );
    } else {
        delete( pool );
    }
}

template< class K, class V >
class TreeTester {
protected:
    const string directory;
    const string fileName;
    ostream& log;
    PagePool* pool;
    Tree< K, V >* tree;
public:
    enum RemoveOrder {
        Forward,
        Reverse,
        Random
    };
    string order2String ( RemoveOrder order ) {
        if (order == Forward) return "Forward";
        else if (order == Reverse) return "Reverse";
        else if (order == Random) return "Random";
        return "Unknown";
    }

    TreeTester() = delete;
    TreeTester( const string dir, const string file, ostream& logStream ) :
        directory( dir ),
        fileName( file ),
        log( logStream ),
        pool( nullptr ),
        tree( nullptr )
    {}
    ~TreeTester() { destroyTree(); destroyPool(); deletePersistentStore(); }
    void createPool() {
        if (pool == nullptr) {
            log << "Constructing persistent page pool on " << directory + "/" + fileName + ".btree" << " ...\n" << flush;
            string poolFile = directory + "/" + fileName + ".btree";
            pool = createPagePool( true, poolFile, BTreePageSize );
        } else {
            log << "Page pool " << directory + "/" + fileName + ".btree" << " already exists!\n" << flush;
        }
    }
    void destroyPool() {
        log << "Deleting page pool ...\n" << flush;
        if (tree != nullptr) {
            log << "B-Tree on " << directory + "/" + fileName + ".btree" << " still exists!\n" << flush;
        }
        if (pool != nullptr) {
            destroyPagePool( true, pool );
            pool = nullptr;
        } else {
            log << "Page pool on " << directory + "/" + fileName + ".btree" << " does not exist!\n" << flush;
        }
    }
    virtual void deletePersistentStore() {
        log << "Deleting persistent store " << directory + "/" + fileName + ".btree" << " ...\n" << flush;
        if (pool != nullptr) {
            log << "Page pool on " << directory + "/" + fileName + ".btree" << " still exists!\n" << flush;
        }
        string poolFile = directory + "/" + fileName + ".btree";
        system( ("DEL " + directory + "\\" + fileName + ".btree").c_str() );
    }
    virtual void createTree() {
        log << "Constructing B-Tree on " << directory + "/" + fileName + ".btree" << " ...\n" << flush;
        if (tree == nullptr) {
            tree = new Tree< K, V >( *pool );
        } else {
            log << "B-Tree on " << directory + "/" + fileName + ".btree" << " already exists!\n" << flush;
        }
    }
    virtual void destroyTree() {
        log << "Deleting B-Tree...\n" << flush;
        if (tree != nullptr) {
            delete( tree );
            tree = nullptr;
        } else {
            log << "B-Tree on page pool " << directory + "/" + fileName + ".btree" << " does not exist!\n" << flush;
        }
    }
    PageDepth treeDepth() const { return(  (tree != nullptr) ? tree->depth() : 0 ); }
    size_t treeSize() const { return( (tree != nullptr) ? tree->size() : 0 ); }
    virtual size_t size() const = 0;
    virtual void validateContent() const {
        log << "Validating content...\n" << flush;
        if (tree == nullptr) {
            log << "B-Tree on page pool " << directory + "/" + fileName + ".btree" << " does not exist!\n";
        } else {
            size_t tSize = treeSize();
            size_t cSize = size();
            if (treeSize() != size()) {
                log << "Size mismatch : B-tree size " << tSize << ", expected " << cSize << ".\n";
            }
        }
    }
    virtual void insert( int count ) {
        log << "Inserting " << count << " keys...\n" << flush;
    };
    virtual void modify( int count ) {
        log << "Modifying " << count << " keys...\n" << flush;
    }
    virtual void remove( int count, RemoveOrder order ) {
        log << "Removing " << count << " keys in " << order2String(order) << " order...\n" << flush;
    }
    virtual void commit() {
        log << "Commit...\n" << flush;
        if (tree != nullptr) {
            tree->commit();
        } else {
            log << "B-Tree on page pool " << directory + "/" + fileName + ".btree" << " does not exist!\n" << flush;
        }
   };
    virtual void recover() {
        log << "Recover...\n" << flush;
        if (tree != nullptr) {
            tree->recover();
        } else {
            log << "B-Tree on page pool " << directory + "/" + fileName + ".btree" << " does not exist!\n" << flush;
        }
    };
    void logTree() const {
        log << "Printing B-Tree content...\n" << flush;
        if (tree != nullptr) {
            this->log << *tree << flush;
        } else {
            log << "B-Tree on page pool " << directory + "/" + fileName + ".btree" << " does not exist!\n" << flush;
        }
    };
}; // class TreeTester

class Uint32Uint32TreeTester : public TreeTester< uint32_t, uint32_t > {
    vector<uint32_t> keys;
    map<uint32_t,uint32_t> content;
    map<uint32_t,uint32_t> commitedContent;
private:
    void removeKey( uint32_t key, uint32_t& errors ) {
        // log << "Removing " << key << ".\n"; log.flush();
        if (!tree->remove( key )) {
            log << "Key " << key << " not found.\n";
            errors += 1;
        }
        content.erase( key );        
    }
public:
    Uint32Uint32TreeTester() = delete;
    Uint32Uint32TreeTester( const string dir, const string file, ostream& logStream ) :
        TreeTester<uint32_t,uint32_t>( dir, file, logStream )
    {}
    void createTree() {
        TreeTester<uint32_t,uint32_t>::createTree();
        content = commitedContent;
        for (auto entry = content.cbegin(); entry != content.cend(); ++entry) keys.push_back( entry->first );
    }
    void destroyTree() {
        TreeTester<uint32_t,uint32_t>::destroyTree();
        keys.clear();
        content.clear();
    }
    void deletePersistentStore() {
        TreeTester<uint32_t,uint32_t>::deletePersistentStore();
        commitedContent.clear();
    }
    size_t size() const { return content.size(); }
    void validateContent() const {
        TreeTester<uint32_t,uint32_t>::validateContent();
        if (tree != nullptr) {
            uint32_t errors = 0;
            for (auto entry = content.cbegin(); entry != content.cend(); ++entry) {
                uint32_t retrieved = tree->retrieve( entry->first );
                if (retrieved != entry->second) {
                    log << "Key " << entry->first << " : Expected " << entry->second << ", retrieved " << retrieved << ".\n";
                    errors += 1;
                }
            }
            if (0 < errors) log << "Detected " << errors << " errors.\n";
        }
    }
    uint32_t generateKey() {
        uint32_t key = generateUint32();
        while (content.count( key ) == 1) key = generateUint32();
        return key;
    };
    void insert( int count ) {
        TreeTester<uint32_t,uint32_t>::insert( count );
        for( int i = 0; i < count; i++) {
            uint32_t key = generateKey();
            uint32_t value = generateUint32();
            tree->insert( key, value );
            keys.push_back( key );
            content[ key ] = value;
        }
    };
    void modify( int count ) {
        TreeTester<uint32_t,uint32_t>::modify( count );
        for( int i = 0; i < count; i++) {
            size_t range = keys.size();
            if (0 < range) {
                uint32_t key = keys[ generateIndex( range ) ];
                uint32_t value = generateUint32();
                tree->replace( key, value );
                content[ key ] = value;
            }
        }
    }
    void remove( int count, RemoveOrder order ) {
        TreeTester<uint32_t,uint32_t>::remove( count, order );
        uint32_t errors = 0;
        size_t range = keys.size();
        if (range < count) count = range;
        if (order == Forward) {
            sort( keys.begin(), keys.end() );
            for ( int i = 0; i < count; i++) removeKey( keys[ i ], errors );
            keys.erase( keys.begin(), keys.begin() + count );
        } else if (order == Reverse) {
            sort( keys.begin(), keys.end() );
            for ( int i = 0; i < count; i++) removeKey( keys[ range - i - 1 ], errors );
            keys.erase( keys.end() - count, keys.end() );
        } else if (order == Random) {
            shuffle( keys.begin(), keys.end(), gen32 );
            for ( int i = 0; i < count; i++) removeKey( keys[ i ], errors );
            keys.erase( keys.begin(), (keys.begin() + count) );
        }
        if (0 < errors) log << "Detected " << errors << " errors.\n";
}
    void commit() {
        TreeTester<uint32_t,uint32_t>::commit();
        commitedContent = content;
    };
    void recover() {
        TreeTester<uint32_t,uint32_t>::recover();
        content = commitedContent;
        keys.clear();
        for (auto entry = content.cbegin(); entry != content.cend(); ++entry) keys.push_back( entry->first );
    };
}; // class Uint32Uint32TreeTester

class Uint16ArrayUint32TreeTester : public TreeTester< uint16_t[], uint32_t > {
protected:
    Uint16ArrayCompare compare;
    vector<vector<uint16_t>*> keys;
    map<vector<uint16_t>*,uint32_t,Uint16ArrayCompare> content;
    map<vector<uint16_t>*,uint32_t,Uint16ArrayCompare> commitedContent;
private:
    void removeKey( vector<uint16_t>* key, uint32_t& errors ) {
        // log << "Removing "; logUint16Array( log, key->data(), key->size() ); log << ".\n"; log.flush();
        if (!tree->remove( key->data(), PageSize( key->size() ))) {
            log << "Key "; logUint16Array( log, key->data(), key->size() ); log << " not found.\n";
            errors += 1;
        }
        content.erase( key );        
    }
public:
    Uint16ArrayUint32TreeTester() = delete;
    Uint16ArrayUint32TreeTester( const string dir, const string file, ostream& logStream ) :
        TreeTester<uint16_t[],uint32_t>( dir, file, logStream )
    {}
    void createTree() {
        TreeTester<uint16_t[],uint32_t>::createTree();
        for (auto entry = commitedContent.cbegin(); entry != commitedContent.cend(); ++entry) {
            content[ new vector<uint16_t>( *entry->first ) ] = entry->second;
        }
        for (auto entry = content.cbegin(); entry != content.cend(); ++entry) keys.push_back( const_cast<vector<uint16_t>*>(entry->first) );
    }
    void destroyTree() {
        TreeTester<uint16_t[],uint32_t>::destroyTree();
        for (auto entry = content.cbegin(); entry != content.cend(); ++entry) {
             delete entry->first;
        }
        keys.clear();
        content.clear();
    }
    void deletePersistentStore() {
        TreeTester<uint16_t[],uint32_t>::deletePersistentStore();
        for (auto entry = commitedContent.cbegin(); entry != commitedContent.cend(); ++entry) {
            delete entry->first;
        }
        commitedContent.clear();
    }
    size_t size() const { return content.size(); }
    void validateContent() const {
        TreeTester<uint16_t[],uint32_t>::validateContent();
        if (tree != nullptr) {
            uint32_t errors = 0;
            for (auto entry = content.cbegin(); entry != content.cend(); ++entry) {
                uint32_t retrieved = tree->retrieve( entry->first->data(), entry->first->size() );
                if (retrieved == 0) {
                    log << "Key ";
                    logUint16Array( log, entry->first->data(), entry->first->size() );
                    log << " not found.\n";
                    errors += 1;
                } else if (retrieved != entry->second) {
                    log << "Key ";
                    logUint16Array( log, entry->first->data(), entry->first->size() );
                    log << " : Expected " << entry->second << ", retrieved " << retrieved << ".\n";
                    errors += 1;
                }
            }
            if (0 < errors) log << "Detected " << errors << " errors.\n";
        }
    }
    vector<uint16_t>* generateKey() const {
        auto key = new vector<uint16_t>;
        generateUint16Array( *key );
        while (content.count( key ) == 1) generateUint16Array( *key );
        return( key );
    };
    void insert( int count ) {
        TreeTester<uint16_t[],uint32_t>::insert( count );
        for( int i = 0; i < count; i++) {
            vector<uint16_t>* key = generateKey();
            uint32_t value = generateUint32();
            tree->insert( key->data(), PageSize( key->size() ), value );
            keys.push_back( key );
            content[ key ] = value;
        }
    };
    void modify( int count ) {
        TreeTester<uint16_t[],uint32_t>::modify( count );
        size_t range = keys.size();
        if (range < count) count = range;
        for( int i = 0; i < count; i++) {
            vector<uint16_t>* key = keys[ generateIndex( count ) ];
            uint32_t value = generateUint32();
            tree->replace( key->data(), PageSize( key->size() ), value );
            content[ key ] = value;
        }
    }
    void remove( int count, RemoveOrder order ) {
        TreeTester<uint16_t[],uint32_t>::remove( count, order );
        uint32_t errors = 0;
        size_t range = keys.size();
        if (range < count) count = range;
        if (order == Forward) {
            sort( keys.begin(), keys.end(), compare );
            for ( int i = 0; i < count; i++) removeKey( keys[ i ], errors );
            keys.erase( keys.begin(), keys.begin() + count );
        } else if (order == Reverse) {
            sort( keys.begin(), keys.end(), compare );
            for ( int i = 0; i < count; i++) removeKey( keys[ range - i - 1 ], errors );
            keys.erase( keys.end() - count, keys.end() );
        } else if (order == Random) {
            shuffle( keys.begin(), keys.end(), gen32 );
            for ( int i = 0; i < count; i++) removeKey( keys[ i ], errors );
            keys.erase( keys.begin(), (keys.begin() + count) );
        }
        if (0 < errors) log << "Detected " << errors << " errors.\n";
    }
    void commit() {
        TreeTester<uint16_t[],uint32_t>::commit();
        for (auto entry = commitedContent.cbegin(); entry != commitedContent.cend(); ++entry) {
            delete entry->first;
        }
        commitedContent.clear();
        for (auto entry = content.cbegin(); entry != content.cend(); ++entry) {
            commitedContent[ new vector<uint16_t>( *entry->first ) ] = entry->second;
        }
    };
    void recover() {
        TreeTester<uint16_t[],uint32_t>::recover();
        keys.clear();
        for (auto entry = content.cbegin(); entry != content.cend(); ++entry) {
            delete entry->first;
        }
        content.clear();
        for (auto entry = commitedContent.cbegin(); entry != commitedContent.cend(); ++entry) {
            content[ new vector<uint16_t>( *entry->first ) ] = entry->second;
        }
        for (auto entry = content.cbegin(); entry != content.cend(); ++entry) keys.push_back( const_cast<vector<uint16_t>*>(entry->first) );
    };
}; // class Uint16ArrayUint32TreeTester

class Uint32Uint16ArrayTreeTester : public TreeTester< uint32_t, uint16_t[] > {
protected:
    vector<uint32_t> keys;
    map<uint32_t,vector<uint16_t>*> content;
    map<uint32_t,vector<uint16_t>*> commitedContent;
private:
    void removeKey( uint32_t key, uint32_t& errors ) {
        // log << "Removing " << key << ".\n"; log.flush();
        if (!tree->remove( key )) {
            log << "Key " << key << " not found.\n";
            errors += 1;
        }
        content.erase( key );        
    }
public:
    Uint32Uint16ArrayTreeTester() = delete;
    Uint32Uint16ArrayTreeTester( const string dir, const string file, ostream& logStream ) :
        TreeTester( dir, file, logStream )
    {}
    void createTree() {
        TreeTester<uint32_t,uint16_t[]>::createTree();
        for (auto entry = commitedContent.cbegin(); entry != commitedContent.cend(); ++entry) {
            content[ entry->first ] = new vector<uint16_t>( *entry->second );
        }
        keys.clear();
        for (auto entry = content.cbegin(); entry != content.cend(); ++entry) keys.push_back( entry->first );
    }
    void destroyTree() {
        TreeTester<uint32_t,uint16_t[]>::destroyTree();
        for (auto entry = content.cbegin(); entry != content.cend(); ++entry) {
            delete entry->second;
        }
        keys.clear();
        content.clear();
    }
    void deletePersistentStore() {
        TreeTester<uint32_t,uint16_t[]>::deletePersistentStore();
        for (auto entry = commitedContent.cbegin(); entry != commitedContent.cend(); ++entry) {
            delete entry->second;
        }
        commitedContent.clear();
    }
    size_t size() const { return content.size(); }
    void validateContent() const {
        TreeTester<uint32_t,uint16_t[]>::validateContent();
        if (tree != nullptr) {
            uint32_t errors = 0;
            for (auto entry = content.cbegin(); entry != content.cend(); ++entry) {
                pair<const uint16_t*,PageIndex> retrieved = tree->retrieve( entry->first );
                if (retrieved.first == nullptr) {
                    log << "Key " << entry->first << " not found.\n";
                    errors += 1;
                } else if (Uint16ArrayCompare::compare( entry->second->data(), entry->second->size(), retrieved.first, retrieved.second )) {
                    log << "Key " << entry->first << " : Expected ";
                    logUint16Array( log, entry->second->data(), entry->second->size() );
                    log << ", retrieved ";
                    logUint16Array( log, retrieved.first, retrieved.second );
                    log << ".\n";
                    errors += 1;
                }
            }
            if (0 < errors) log << "Detected " << errors << " errors.\n";
        }
    }
    uint32_t generateKey() {
        uint32_t key = generateUint32();
        while (content.count( key ) == 1) key = generateUint32();
        return key;
    };
    vector<uint16_t>* generateValue() const {
        auto value = new vector<uint16_t>;
        generateUint16Array( *value );
        return( value );
    };
    void insert( int count ) {
        TreeTester<uint32_t,uint16_t[]>::insert( count );
        for( int i = 0; i < count; i++) {
            uint32_t key = generateKey();
            vector<uint16_t>* value = generateValue();
            tree->insert( key, value->data(), PageSize( value->size() ) );
            keys.push_back( key );
            content[ key ] = value;
        }
    };
    void modify( int count ) {
        TreeTester<uint32_t,uint16_t[]>::modify( count );
        size_t range = keys.size();
        if (range < count) count = range;
        for( int i = 0; i < count; i++) {
            uint32_t key = keys[ generateIndex( range ) ];
            vector<uint16_t>* value = generateValue();
            tree->replace( key, value->data(), PageSize( value->size() ) );
            vector<uint16_t>* oldValue = content[ key ];
            content[ key ] = value;
            delete oldValue;
        }
    }

    void remove( int count, RemoveOrder order ) {
        TreeTester<uint32_t,uint16_t[]>::remove( count, order );
        uint32_t errors = 0;
        size_t range = keys.size();
        if (range < count) count = range;
        if (order == Forward) {
            sort( keys.begin(), keys.end() );
            for ( int i = 0; i < count; i++) removeKey( keys[ i ], errors );
            keys.erase( keys.begin(), keys.begin() + count );
        } else if (order == Reverse) {
            sort( keys.begin(), keys.end() );
            for ( int i = 0; i < count; i++) removeKey( keys[ range - i - 1 ], errors );
            keys.erase( keys.end() - count, keys.end() );
        } else if (order == Random) {
            shuffle( keys.begin(), keys.end(), gen32 );
            for ( int i = 0; i < count; i++) removeKey( keys[ i ], errors );
            keys.erase( keys.begin(), (keys.begin() + count) );
        }
        if (0 < errors) log << "Detected " << errors << " errors.\n";
    }
    void commit() {
        TreeTester<uint32_t,uint16_t[]>::commit();
        for (auto entry = commitedContent.cbegin(); entry != commitedContent.cend(); ++entry) {
            delete entry->second;
        }
        commitedContent.clear();
        for (auto entry = content.cbegin(); entry != content.cend(); ++entry) {
            commitedContent[ entry->first ] = new vector<uint16_t>( *entry->second );
        }
    };
    void recover() {
        TreeTester<uint32_t,uint16_t[]>::recover();
        keys.clear();
        for (auto entry = content.cbegin(); entry != content.cend(); ++entry) {
            delete entry->second;
        }
        content.clear();
        for (auto entry = commitedContent.cbegin(); entry != commitedContent.cend(); ++entry) {
            content[ entry->first ] = new vector<uint16_t>( *entry->second );
        }
        for (auto entry = content.cbegin(); entry != content.cend(); ++entry) keys.push_back( entry->first );
    };
}; // class Uint32Uint16ArrayTreeTester

class Uint16ArrayUint16ArrayTreeTester : public TreeTester< uint16_t[], uint16_t[] > {
protected:
    Uint16ArrayCompare compare;
    vector<vector<uint16_t>*> keys;
    map<vector<uint16_t>*,vector<uint16_t>*,Uint16ArrayCompare> content;
    map<vector<uint16_t>*,vector<uint16_t>*,Uint16ArrayCompare> commitedContent;
private:
    void removeKey( vector<uint16_t>* key, uint32_t& errors ) {
        // log << "Removing "; logUint16Array( log, key->data(), key->size() ); log << ".\n"; log.flush();
        if (!tree->remove( key->data(), PageSize( key->size() ))) {
            log << "Key "; logUint16Array( log, key->data(), key->size() ); log << " not found.\n";
            errors += 1;
        }
        content.erase( key );        
    }
public:
    Uint16ArrayUint16ArrayTreeTester() = delete;
    Uint16ArrayUint16ArrayTreeTester( const string dir, const string file, ostream& logStream ) :
        TreeTester<uint16_t[],uint16_t[]>( dir, file, logStream )
    {}
    void createTree() {
        TreeTester<uint16_t[],uint16_t[]>::createTree();
        for (auto entry = commitedContent.cbegin(); entry != commitedContent.cend(); ++entry) {
            content[ new vector<uint16_t>( *entry->first ) ] = new vector<uint16_t>( *entry->second );
        }
        for (auto entry = content.cbegin(); entry != content.cend(); ++entry) keys.push_back( const_cast<vector<uint16_t>*>(entry->first) );
    }
    void destroyTree() {
        TreeTester<uint16_t[],uint16_t[]>::destroyTree();
        for (auto entry = content.cbegin(); entry != content.cend(); ++entry) {
            delete entry->first;
            delete entry->second;
        }
        keys.clear();
        content.clear();
    }
    void deletePersistentStore() {
        TreeTester<uint16_t[],uint16_t[]>::deletePersistentStore();
        for (auto entry = commitedContent.cbegin(); entry != commitedContent.cend(); ++entry) {
            delete entry->first;
            delete entry->second;
        }
        commitedContent.clear();
    }
    size_t size() const { return content.size(); }
    void validateContent() const {
        TreeTester<uint16_t[],uint16_t[]>::validateContent();
        if (tree != nullptr) {
            uint32_t errors = 0;
            for (auto entry = content.cbegin(); entry != content.cend(); ++entry) {
                pair<const uint16_t*,PageIndex> retrieved = tree->retrieve( entry->first->data(), entry->first->size() );
                if (retrieved.first == nullptr) {
                    log << "Key ";
                    logUint16Array( log, entry->first->data(), entry->first->size() );
                    log << " not found.\n";
                    errors += 1;
                } else if (Uint16ArrayCompare::compare( entry->second->data(), entry->second->size(), retrieved.first, retrieved.second )) {
                    log << "Key ";
                    logUint16Array( log, entry->first->data(), entry->first->size() );
                    log << " : Expected ";
                    logUint16Array( log, entry->second->data(), entry->second->size() );
                    log << ", retrieved ";
                    logUint16Array( log, retrieved.first, retrieved.second );
                    log << ".\n";
                    errors += 1;
                }
            }
            if (0 < errors) log << "Detected " << errors << " errors.\n";
        }
    }
    vector<uint16_t>* generateKey() const {
        auto key = new vector<uint16_t>;
        generateUint16Array( *key );
        while (content.count( key ) == 1) generateUint16Array( *key );
        return( key );
    };
    vector<uint16_t>* generateValue() const {
        auto value = new vector<uint16_t>;
        generateUint16Array( *value );
        return( value );
    };
    void insert( int count ) {
        TreeTester<uint16_t[],uint16_t[]>::insert( count );
        for( int i = 0; i < count; i++) {
            vector<uint16_t>* key = generateKey();
            vector<uint16_t>* value = generateValue();
            tree->insert( key->data(), PageSize( key->size() ), value->data(), PageSize( value->size() ) );
            keys.push_back( key );
            content[ key ] = value;
        }
    };
    void modify( int count ) {
        TreeTester<uint16_t[],uint16_t[]>::modify( count );
        size_t range = keys.size();
        if (range < count) count = range;
        for( int i = 0; i < count; i++) {
            vector<uint16_t>* key = keys[ generateIndex( range ) ];
            vector<uint16_t>* value = generateValue();
            tree->replace( key->data(), PageSize( key->size() ), value->data(), PageSize( value->size() ) );
            vector<uint16_t>* oldValue = content[ key ];
            content[ key ] = value;
            delete oldValue;
        }
    }

    void remove( int count, RemoveOrder order ) {
        TreeTester<uint16_t[],uint16_t[]>::remove( count, order );
        uint32_t errors = 0;
        size_t range = keys.size();
        if (range < count) count = range;
        if (order == Forward) {
            sort( keys.begin(), keys.end(), compare );
            for ( int i = 0; i < count; i++) removeKey( keys[ i ], errors );
            keys.erase( keys.begin(), keys.begin() + count );
        } else if (order == Reverse) {
            sort( keys.begin(), keys.end(), compare );
            for ( int i = 0; i < count; i++) removeKey( keys[ range - i - 1 ], errors );
            keys.erase( keys.end() - count, keys.end() );
        } else if (order == Random) {
            shuffle( keys.begin(), keys.end(), gen32 );
            for ( int i = 0; i < count; i++) removeKey( keys[ i ], errors );
            keys.erase( keys.begin(), (keys.begin() + count) );
        }
        if (0 < errors) log << "Detected " << errors << " errors.\n";
    }
    void commit() {
        TreeTester<uint16_t[],uint16_t[]>::commit();
        for (auto entry = commitedContent.cbegin(); entry != commitedContent.cend(); ++entry) {
            delete entry->first;
            delete entry->second;
        }
        commitedContent.clear();
        for (auto entry = content.cbegin(); entry != content.cend(); ++entry) {
            commitedContent[ new vector<uint16_t>( *entry->first ) ] = new vector<uint16_t>( *entry->second );
        }
    };
    void recover() {
        TreeTester<uint16_t[],uint16_t[]>::recover();
        keys.clear();
        for (auto entry = content.cbegin(); entry != content.cend(); ++entry) {
            delete entry->first;
            delete entry->second;
        }
        content.clear();
        for (auto entry = commitedContent.cbegin(); entry != commitedContent.cend(); ++entry) {
            content[ new vector<uint16_t>( *entry->first ) ] = new vector<uint16_t>( *entry->second );
        }
        for (auto entry = content.cbegin(); entry != content.cend(); ++entry) keys.push_back( const_cast<vector<uint16_t>*>(entry->first) );
    };
}; // class Uint16ArrayUint16ArrayTreeTester

template< class K, class V >
void doTest( TreeTester<K,V>& tester, size_t count, ofstream& log ) {
    try {
        tester.createPool();
        tester.createTree();
        tester.commit();
/*
*/
        tester.insert( count );
        tester.validateContent();
        tester.remove( count, TreeTester<K,V>::Forward );
        tester.validateContent();
        tester.logTree();
        tester.destroyTree();
        tester.createTree();
        tester.insert( count );
        tester.validateContent();
        tester.remove( count, TreeTester<K,V>::Reverse );
        tester.validateContent();
        tester.logTree();
        tester.destroyTree();
        tester.createTree();
        tester.insert( count );
        tester.validateContent();
        tester.remove( count, TreeTester<K,V>::Random );
        tester.validateContent();
        tester.logTree();
        tester.destroyTree();
        tester.createTree();
        tester.insert( count / 10 );
        tester.commit();
        tester.validateContent();
        tester.insert( count - (count / 10) );
        tester.commit();
        tester.validateContent();
        tester.modify( count / 2 );
        tester.validateContent();
        tester.remove( count - (count / 4), TreeTester<K,V>::Random );
        tester.validateContent();
        tester.recover();
        tester.validateContent();
        tester.remove( count / 2, TreeTester<K,V>::Random );
        tester.validateContent();
        tester.modify( count / 2 );
        tester.validateContent();
        tester.insert( count / 2 );
        tester.validateContent();
        tester.destroyTree();
        tester.createTree();
        tester.validateContent();
        tester.destroyTree();
        tester.destroyPool();
        tester.createPool();
        tester.createTree();
        tester.validateContent();
        tester.remove( count / 4, TreeTester<K,V>::Random );
        tester.validateContent();
        tester.recover();
        tester.validateContent();
/*
*/
    }
    catch ( string message ) {
        log << "Exception : " << message << "\n";
        // tester.logTree();
    }
    catch (...) {
        log << "Exception (...)\n";
        // tester.logTree();
    }
    log.flush();
}

int main(int argc, char* argv[]) {
    system( "RMDIR /S /Q testBTreeValidity" );
    system( "MKDIR testBTreeValidity" );
    ofstream log;
    log.open( "testBTreeValidity\\log.txt" );
    size_t count = 0;
    if (2 <= argc) count = stoi( argv[ 1 ] );
    for (int arg = 2; arg < argc; arg++) {
        if (string( "Uint32Uint32" ) == argv[ arg ]) {
            log << "32-bit unsigned integer key to 32-bit unsigned integer B-Tree...\n" << flush;
            Uint32Uint32TreeTester tester( "testBTreeValidity", "Uint32Uint32", log );
            doTest( tester, count, log );
        }
        if (string( "Uint16ArrayUint32" ) == argv[ arg ]) {
            log << "16-bit unsigned integer array key to 32-bit unsigned integer B-Tree.\n" << flush;
            Uint16ArrayUint32TreeTester tester( "testBTreeValidity", "Uint16ArrayUint32", log );
            doTest( tester, count, log );
        }
        if (string( "Uint32Uint16Array" ) == argv[ arg ]) {
            log << "32-bit unsigned integer key to 16-bit unsigned integer array B-Tree.\n" << flush;
            Uint32Uint16ArrayTreeTester tester( "testBTreeValidity", "Uint32Uint16Array", log );
            doTest( tester, count, log );
        }
        if (string( "Uint16ArrayUint16Array" ) == argv[ arg ]) {
            log << "16-bit unsigned integer array key to 16-bit unsigned integer array B-Tree.\n" << flush;
            Uint16ArrayUint16ArrayTreeTester tester( "testBTreeValidity", "Uint16ArrayUint16Array", log );
            doTest( tester, count, log );
        }
        log << "Done...\n\n";
    }
    log.flush();
    log.close();
};
