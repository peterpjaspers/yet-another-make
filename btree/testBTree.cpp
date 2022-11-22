#include "BTree.h"
#include "PersistentPagePool.h"
#include <iostream>
#include <fstream>
#include <map>
#include <chrono>
#include <string>

// B-tree test program.
//
// Test is driven by variable length argument list. Each argument defining a property of or
// operation on the B-tree. Each command is a single letter, optionally followed (concatenated)
// with a character or decimal integer value.
//
// Commands:
//    array keys        array keys (otherwise scalar keys)
//    array values      array values (otherwise scalar values)
//    persistent        persistent store (otherwise in memory)
//    transaction       transaction mode updates (otherwise in-place)
//    create NNN (name) create B-tree with page size NNN in file name (if persistent)
//    commit            commit modifications (transaction succeeds)
//    recover           recover from modifications (transaction fails)
//    stream            stream content of B-tree to human readable text file (testBTree.txt)
//    generate NNN      generate NNN key-value pairs without insertion in B-tree
//    insert NNN        insert NNN entries
//    retreive NNN      retreive NNN values
//    modify NNN        modify NNN values
//    delete NNN        delete NNN entries
//    verify            verify result of all B-tree modifications of next update request (i,u or d) command.
//    start             start time monitoring
//    stop              stop time monitoring and output elapsed duration since previous sart
//
// To be effective, the array, persistent and transaction commands should be issued before the create command.
// The all other commands must be issued after the create command.
// The generate command may be used to synchronize entry generation with a persistent B-tree or to minimize
// key-value generation overhead for performance measurements.
//
// Example: testBTree array keys persistent tranascation create 1024 insert 1000 commit delete 300 stream
// Creates a 1024 byte page, PersistentTransaction B-tree with character array keys.
// Inserts 1000 entries, commits, deletes 300 entries and finally streams the resulting
// B-tree to a text file.
//

using namespace BTree;
using namespace std;

const int BTreePageSize = 512;
const int MaxIterations = 1000000;

const int MinKeyString = 2;
const int MaxKeyString = 15;
const int MinValueString = 4;
const int MaxValueString = 15;

inline int32_t generateIntKey() {
    return (((rand() * 7919) + rand()) % 10000000);    
}
void generateStringKey( char value[] ) {
    static char characters[ 26 + 26 + 10 + 1 ] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    int n = (MinKeyString  + (rand() % (MaxKeyString - MinKeyString)));
    for (int i = 0; i < n; i++) value[ i ] = characters[ (rand() % (26 + 26 + 10)) ];
    value[ n ] = 0;
}
inline int generateKeyIndex( size_t keyCount ) {
    return (((rand() * 7919) + rand()) % keyCount);
}
inline int32_t generateIntValue() {
    return (rand() % 10000);    
}
void generateStringValue( char value[] ) {
    static char characters[ 26 + 26 + 10 + 1 ] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    int n = (MinValueString  + (rand() % (MaxValueString - MinValueString)));
    for (int i = 0; i < n; i++) value[ i ] = characters[ (rand() % (26 + 26 + 10)) ];
    value[ n ] = 0;
}

class TreeTester {
protected:
    PagePool& pool;
    ostream& outputStream;
public:
    TreeTester() = delete;
    TreeTester( PagePool& pagePool, ostream& stream ) : pool( pagePool ), outputStream( stream ) {}
    virtual int size() = 0;
    virtual void generate() = 0;
    virtual void insert() = 0;
    virtual bool retrieve( int iteration ) = 0;
    virtual void modify() = 0;
    virtual bool remove( int iteration ) = 0;
    virtual void stream() = 0;
    virtual void commit() = 0;
    virtual void recover() = 0;
    virtual bool verify() = 0;
};

class IntIntTreeTester : virtual public TreeTester {
    Tree< int32_t, int32_t > tree;
    map<int32_t,int32_t> values;
    vector<int32_t> keys;
    int32_t lastKey;
public:
    IntIntTreeTester( PagePool& pagePool, ostream& stream, UpdateMode mode ) :
    TreeTester( pagePool, stream ), tree( pagePool, mode ) {};
    int size() { return keys.size(); }
    void generate() {
        int32_t key = generateIntKey();
        if (values.find( key ) == values.end()) {
            values[ key ] = generateIntValue();
            keys.push_back( key );
        }
    };
    void insert() {
        generate();
        int32_t key = keys.back();
        int32_t value = values[ key ];
        tree.insert( key, value );
        lastKey = key;
    };
    bool retrieve( int iteration ) {
        int32_t key( keys[ iteration ] );
        int32_t value = tree.retrieve( key );
        bool retrieved = ( (value == values[ key ]) ? true : false );
        if (!retrieved) outputStream << "Retrieve error for key " << key << " retrieved " << value << " != exepected " << values[ key ] << " [ " << iteration << " ]\n" << flush;
        return retrieved;
    };
    void modify() {
        size_t keyCount = keys.size();
        if (0 < keyCount) {
            int index = generateKeyIndex( keyCount );
            uint32_t key( keys[ index ] );
            int32_t value = generateIntValue();
            tree.insert( key, value );
            lastKey = key;
        }
    };
    bool remove( int iteration ) {
        size_t keyCount = keys.size();
        if (0 < keyCount) {
            int index = generateKeyIndex( keyCount );
            int32_t key = keys[ index ];
            bool removed = ( (tree.remove( key )) ? true : false );
            keys.erase( keys.begin() + index );
            if (!removed) outputStream << "Failed to remove key " << key << " [ " << iteration << " ]\n" << flush;
            lastKey = key;
            return removed;
        }
        return false;
    };
    void stream() { this->outputStream << tree << flush; };
    void commit() { tree.commit(); };
    void recover() { tree.recover(); };
    bool verify() {
        for (int k = 0; k < keys.size(); k++) {
            int32_t key( keys[ k ] );
            if (tree.retrieve( keys[ k ] ) != values[ key ]) {
                outputStream << "Expecting " << key << " -> " << values[ key ] << " != " << tree.retrieve( keys[ k ] ) << ", last key " << lastKey << "\n";
                return false;
            }
        }
        return true;
    }
};

class IntCStringTreeTester : virtual public TreeTester {
    Tree< int32_t, char, false, true > tree;
    map<int32_t,string> values;
    vector<int32_t> keys;
public:
    IntCStringTreeTester( PagePool& pagePool, ostream& stream, UpdateMode mode ) :
    TreeTester( pagePool, stream ), tree( pagePool, mode ) {};
    int size() { return keys.size(); }
    void generate() {
        char value[ MaxValueString + 1 ];
        int32_t key = generateIntKey();
        if (values.find( key ) == values.end()) {
            generateStringValue( value );
            string valueString( value );
            values[ key ] = valueString;
            keys.push_back( key );
        }
    }
    void insert() {
        generate();
        int32_t key = keys.back();
        string valueString( values[ key ] );
        tree.insert( key, valueString.c_str(), valueString.size() );
    };
    bool retrieve( int iteration ) {
        uint32_t key( keys[ iteration ] );
        pair<const char*,int32_t> result = tree.retrieve( key );
        string valueString( result.first, result.second );
        bool retrieved = ( (valueString == values[ key ]) ? true : false );
        if (!retrieved) outputStream << "Retrieve error for key " << key << " retrieved " << valueString << " != expected " << values[ key ] << " [ " << iteration << " ]\n" << flush;
        return retrieved;
    };
    void modify() {
        size_t keyCount = keys.size();
        if (0 < keyCount) {
            int index = generateKeyIndex( keyCount );
            uint32_t key( keys[ index ] );
            char value[ MaxValueString + 1 ];
            generateStringValue( value );
            tree.insert( key, value, strlen( value ) );
        }
    };
    bool remove( int iteration ) {
        size_t keyCount = keys.size();
        if (0 < keyCount) {
            int index = generateKeyIndex( keyCount );
            uint32_t key( keys[ index ] );
            bool removed = ( (tree.remove( key )) ? true : false );
            keys.erase( keys.begin() + index );
            if (!removed) outputStream << "Failed to remove key " << key << " [ " << iteration << " ]\n" << flush;
            return removed;
        }
        return false;
    };
    void stream() { this->outputStream << tree << flush; };
    void commit() { tree.commit(); };
    void recover() { tree.recover(); };
    bool verify() {
        for (int k = 0; k < keys.size(); k++) {
            int32_t key( keys[ k ] );
            pair<const char*, uint16_t> value = tree.retrieve( keys[ k ] );
            if (string(value.first, value.second) != values[ key ]) {
                outputStream << "Expecting " << key << " -> " << values[ key ] << " != " << string(value.first, value.second) << "\n";
                return false;
            }
        }
        return true;
    }
};

class CStringIntTreeTester : virtual public TreeTester {
    Tree< char, int32_t, true, false > tree;
    map<string,int32_t> values;
    vector<string> keyStrings;
    vector<int32_t> keys;
    uint32_t index;
public:
    CStringIntTreeTester( PagePool& pagePool, ostream& stream, UpdateMode mode ) :
    TreeTester( pagePool, stream ), tree( pagePool, mode ), index( 0 ) {};
    int size() { return keys.size(); }
    void generate() {
        char key[ MaxKeyString + 1 ];
        generateStringKey( key );
        string keyString( key );
        if (values.find( keyString ) == values.end()) {
            int32_t value = generateIntValue();
            values[ keyString ] = value;
            keyStrings.push_back( keyString );
            keys.push_back( index++ );
        }
    }
    void insert() {
        generate();
        const string& keyString = keyStrings[ keys.back() ];
        int32_t value = values[ keyString ];
        tree.insert( keyString.c_str(), keyString.size(), value );
    };
    bool retrieve( int iteration ) {
        const string& keyString = keyStrings[ keys[ iteration ] ];
        int32_t value = tree.retrieve( keyString.c_str(), keyString.size() );
        bool retrieved = ( (value == values[ keyString ]) ? true : false );
        if (!retrieved) outputStream << "Retrieve error for key " << keyString << " retrieved " << value << " != expected " << values[ keyString ] << " [ " << iteration << " ]\n" << flush;
        return retrieved;
    };
    void modify() {
        size_t keyCount = keys.size();
        if (0 < keyCount) {
            uint32_t index = generateKeyIndex( keyCount );
            const string& keyString = keyStrings[ keys[ index ] ];
            int32_t value = generateIntValue();
            values[ keyString ] = value;
            tree.insert( keyString.c_str(), keyString.size(), value );
        }
    };
    bool remove( int iteration ) {
        size_t keyCount = keys.size();
        if (0 < keyCount) {
            uint32_t index = generateKeyIndex( keyCount );
            const string& keyString = keyStrings[ keys[ index ] ];
            bool removed = tree.remove( keyString.c_str(), keyString.size() );
            keys.erase( keys.begin() + index );
            if (!removed) outputStream << "Failed to remove key " << keyString << " [ " << iteration << " ]\n" << flush;
            return removed;
        }
        return false;
    };
    void stream() { this->outputStream << tree << flush; };
    void commit() { tree.commit(); };
    void recover() { tree.recover(); };
    bool verify() {
        for (int k = 0; k < keys.size(); k++) {
            const string& keyString = keyStrings[ keys[ index ] ];
            int32_t value = tree.retrieve( keyString.c_str(), keyString.size() );
            if (value != values[ keyString ]) {
                outputStream << "Expecting " << keyString << " -> " << values[ keyString ] << " != " << value << "\n";
                return false;
            }
        }
        return true;
    }
};

class CStringCStringTreeTester : virtual public TreeTester {
    Tree< char, char, true, true > tree;
    map<string,string> values;
    vector<string> keyStrings;
    vector<uint32_t> keys;
    uint32_t index;
public:
    CStringCStringTreeTester( PagePool& pagePool, ostream& stream, UpdateMode mode ) :
    TreeTester( pagePool, stream ), tree( pagePool, mode ), index( 0 ) {};
    int size() { return keys.size(); }
    void generate() {
        char key[ MaxKeyString + 1 ];
        generateStringKey( key );
        string keyString( key );
        if (values.find( keyString ) == values.end()) {
            char value[ MaxValueString + 1 ];
            generateStringValue( value );
            string valueString( value );
            values[ keyString ] = valueString;
            keyStrings.push_back( keyString );
            keys.push_back( index++ );
        }
    }
    void insert() {
        generate();
        const string& keyString = keyStrings[ keys.back() ];
        string valueString( values[ keyString ] );
        tree.insert( keyString.c_str(), keyString.size(), valueString.c_str(), valueString.size() );
    };
    bool retrieve( int iteration ) {
        const string& keyString = keyStrings[ keys[ iteration ] ];
        pair<const char*, uint16_t> value = tree.retrieve( keyString.c_str(), keyString.size() );
        string valueString( value.first, value.second );
        bool retrieved = ( (valueString == values[ keyString ]) ? true : false );
        if (!retrieved) outputStream << "Retrieve error for key " << keyString << " retrieved " << valueString << " != expected " << values[ keyString ] << " [ " << iteration << " ]\n" << flush;
        return retrieved;
    };
    void modify() {
        size_t keyCount = keys.size();
        if (0 < keyCount) {
            uint32_t index = generateKeyIndex( keyCount );
            const string& keyString = keyStrings[ keys[ index ] ];
            char value[ MaxValueString + 1 ];
            generateStringValue( value );
            string valueString( value );
            values[ keyString ] = valueString;
            tree.insert( keyString.c_str(), keyString.size(), valueString.c_str(), valueString.size() );
        }
    };
    bool remove( int iteration ) {
        size_t keyCount = keys.size();
        if (0 < keyCount) {
            uint32_t index = generateKeyIndex( keyCount );
            const string& keyString = keyStrings[ keys[ index ] ];
            bool removed = tree.remove( keyString.c_str(), keyString.size() );
            keys.erase( keys.begin() + index );
            if (!removed) outputStream << "Failed to remove key " << keyString << " [ " << iteration << " ]\n" << flush;
            return removed;
        }
        return false;
    };
    void stream() { this->outputStream << tree << flush; };
    void commit() { tree.commit(); };
    void recover() { tree.recover(); };
    bool verify() {
        for (int k = 0; k < keys.size(); k++) {
            const string& keyString = keyStrings[ keys[ index ] ];
            pair<const char*, uint16_t> value = tree.retrieve( keyString.c_str(), keyString.size() );
            string valueString(value.first, value.second);
            if (valueString != values[ keyString ]) {
                outputStream << "Expecting " << keyString << " -> " << values[ keyString ] << " != " << valueString << "\n";
                return false;
            }
        }
        return true;
    }
};

PagePool* createPagePool( bool persistent, string path, PageSize pageSize ) {
    if ( persistent ) {
        // Determine stored page size (if any)...
        PageSize storedPageSize = PersistentPagePool::pageCapacity( path );
        return new PersistentPagePool( ((0 < storedPageSize) ? storedPageSize : pageSize), path );
    } else {
        return new PagePool( pageSize );
    }
}

int main(int argc, char* argv[]) {
    int pageSize = BTreePageSize;
    bool arrayKeys = false;
    bool arrayValues = false;
    bool persistentStore = false;
    bool verify = false;
    UpdateMode mode = InPlace;
    auto start = chrono::steady_clock::now();;
    ofstream stream;
    stream.open( "testBTree.txt" );
    try {
        TreeTester* tester = nullptr;
        for (int i = 1; i < argc; i++) {
            string command( argv[ i ] );
            if (command ==  "file") {
                string fileName( argv[ ++i ] );
                stream << "Streaming to " << fileName << flush;
                stream.close();
                stream.open( fileName );
            } else if (command ==  "array") {
                string argument( argv[ ++i ] );
                if (argument == "keys") arrayKeys = true; else arrayValues = true;
            } else if (command ==  "scalar") {
                string argument( argv[ ++i ] );
                if (argument == "keys") arrayKeys = false; else arrayValues = false;
            } else if (command ==  "persistent") {
                persistentStore = true;
            } else if (command == "verify") {
                verify = true;
            } else if (command == "transaction") {
                mode = ( (persistentStore) ? PersistentTransaction : MemoryTransaction);
            } else if (command ==  "create") {
                pageSize = PageSize( min( max( stoi( argv[ ++i ] ), int( MinPageSize)  ), int( MaxPageSize ) ) );
                string persistentFile;
                if (persistentStore) persistentFile = argv[ ++i ];
                if (!arrayKeys && !arrayValues) {
                    stream << "--- int -> int ---\n";
                    PagePool* pagePool = createPagePool( persistentStore, persistentFile, pageSize );
                    tester = new IntIntTreeTester( *pagePool, stream, mode );
                } else if (!arrayKeys && arrayValues) {
                    stream << "--- int -> C-string ---\n";
                    PagePool* pagePool = createPagePool( persistentStore, persistentFile, pageSize );
                    tester = new IntCStringTreeTester( *pagePool, stream, mode );
                } else if (arrayKeys && !arrayValues) {
                    stream << "--- C-string -> int ---\n";
                    PagePool* pagePool = createPagePool( persistentStore, persistentFile, pageSize );
                    tester = new CStringIntTreeTester( *pagePool, stream, mode );
                } else if (arrayKeys && arrayValues) {
                    stream << "--- C-string -> C-string ---\n";
                    PagePool* pagePool = createPagePool( persistentStore, persistentFile, pageSize );
                    tester = new CStringCStringTreeTester( *pagePool, stream, mode );
                }
            } else if (command ==  "commit") {
                if (!tester) throw "No B-tree defined!";
                stream << "Transaction commit\n" << flush;
                tester->commit();
            } else if (command ==  "recover") {
                if (!tester) throw "No B-tree defined!";
                stream << "Transaction recover\n" << flush;
                tester->recover();
            } else if (command ==  "stream") {
                if (!tester) throw "No B-tree defined!";
                tester->stream();
            } else if (command ==  "generate") {
                int generations = min( stoi( argv[ ++i ] ), MaxIterations );
                stream << generations << " key-value pairs generated...\n" << flush;
                for (int i = 0; i < generations; i++) tester->generate();
            } else if (command == "insert") {
                if (!tester) throw "No B-tree defined!";
                int insertions = min( stoi( argv[ ++i ] ), MaxIterations );
                if (verify) stream << "Verifying ";
                stream << insertions << " insertions...\n" << flush;
                for (int i = 0; i < insertions; i++) {
                    tester->insert();
                    if (verify) if (!tester->verify()) {
                        stream << "Inconsistent B-tree content at iteration " << i << "\n";
                        verify = false;
                    }
                }
                verify = false;
            } else if (command == "retrieve") {
                if (!tester) throw "No B-tree defined!";
                int retrievals = min( min( stoi( argv[ ++i ] ), tester->size() ), MaxIterations );
                stream << retrievals << " retrievals...\n";
                int retrieved = 0;
                for (int i = 0; i < retrievals; i++) if (tester->retrieve( i )) retrieved += 1;
                if (0 < (retrievals - retrieved)) {
                    stream << (retrievals - retrieved) << " retrieval failures, " << setprecision(2) << fixed << (100.0 * (retrievals - retrieved)) / retrievals << " %.\n" << flush;
                }
            } else if (command == "modify") {
                if (!tester) throw "No B-tree defined!";
                int modifications = min( min( stoi( argv[ ++i ] ), tester->size() ), MaxIterations );
                if (verify) stream << "Verifying ";
                stream << modifications << " modifications...\n";
                for (int i = 0; i < modifications; i++) {
                    tester->modify();
                    if (verify) if (!tester->verify()) {
                        stream << "Inconsistent B-tree content at iteration " << i << "\n";
                        verify = false;
                    }
                }
                verify = false;
            } else if (command == "delete") {
                if (!tester) throw "No B-tree defined!";
                int deletions = min( min( stoi( argv[ ++i ] ), tester->size() ), MaxIterations );
                int deleted = 0;
                if (verify) stream << "Verifying ";
                stream << deletions << " deletions...\n" << flush;
                for (int i = 0; i < deletions; i++) {
                    if (tester->remove( i )) deleted += 1;
                    if (verify) if (!tester->verify()) {
                        stream << "Inconsistent B-tree content at iteration " << i << "\n";
                        verify = false;
                    }
                }
                if (0 < (deletions - deleted)) {
                    stream << (deletions - deleted) << " deletion failures, " << setprecision(2) << fixed << (100.0 * (deletions - deleted)) / deletions << " %.\n" << flush;
                }
                verify = false;
            } else if (command == "start") {
                start = chrono::steady_clock::now();
            } else if (command == "stop") {
                auto end = chrono::steady_clock::now();
                chrono::duration<double> elapsed = (end  -start);
                stream << "Elapsed " << elapsed.count() << " seconds\n";
            } else {
                throw (command + " - Invalid argument");
            }
        }
    }
    catch ( string message ) {
        stream << message << "\n";
    }
    catch (...) {
        stream << "Exception!\n";
    }
    stream << "Done...\n";
    stream.close();
}