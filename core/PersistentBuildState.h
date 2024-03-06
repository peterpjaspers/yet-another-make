#pragma once

#include <memory>
#include <filesystem>
#include <map>
#include <unordered_set>

namespace BTree 
{
    class PersistentPagePool;
    class Forest;
    template<class K> class ValueReader;
    template<class K> class StreamingTree;
    typedef std::uint32_t TreeIndex;
}

namespace YAM
{
    class ExecutionContext;
    class IPersistable;

    // The YAM build state consists of the nodes and repositories in an
    // ExecutionContext. 
    // Happy flow usage:
    //      PersistentBuildState pstate(directory, context);
    //      pstate.retrieve();
    //      // Add/modify/remove nodes and/or repos in context.
    //      pstate.store();
    // 
    // Non-happy flow usage:
    //      PersistentBuildState pstate(directory, context);
    //      pstate.retrieve();
    //      // Add/modify/remove nodes and/or repos in context.
    //      // Discover corruption of the build state, e.g. changes in a 
    //      // build file introduced a cycle in the node graph.      
    //      pstate.rollback();
    //
    class __declspec(dllexport) PersistentBuildState
    {
    public:
        friend class SharedPersistableReader;
        friend class SharedPersistableWriter;

        typedef uint64_t Key;

        // Construct for storage of build state in given directory.
        PersistentBuildState(
            std::filesystem::path const& directory,
            ExecutionContext* context);

        ~PersistentBuildState();

        // Retrieve the build state.
        // Time complexity: O(N) where N is the number of objects in the build
        // state.
        // Post: context contains the retrieved build state.
        void retrieve();

        // Store the build state changes applied since last commit().
        // Time complexity: O(N) where N is the number of new/modified/removed
        // objects in the build state since previous store().
        // Return N.
        // Post:
        //     store() != -1 : commit succeeded.
        //     store() == -1 : commit failed. The build state has been rolled 
        //                     back to the state at last successfull commit.
        std::size_t store();

        // Rollback the build state to its state at last successfull commit.
        void rollback();

        // For testing purposes.
        // An object is pending delete when it was requested to be deleted
        // while other stored objects were stil referencing it.
        // The object will be deleted when its reference count drops to 0.
        bool isPendingDelete(std::string const &name) const;

        void logState(ILogBook &logBook);

    private:
        // retrieve for use by SharedPersistableReader
        std::shared_ptr<IPersistable> getObject(Key key);
        // store for use by SharedPersistableWriter
        Key getKey(std::shared_ptr<IPersistable> const& object);

        void reset();
        void retrieveAll();
        void retrieveKey(Key key);
        void retrieveKey(Key key, BTree::ValueReader<Key>& reader);

        Key bindToKey(std::shared_ptr<IPersistable> const& object);
        Key allocateKey(IPersistable* object);
        void store(Key key,std::shared_ptr<IPersistable> const& object);

        bool remove(Key key, std::shared_ptr<IPersistable> const& object);
        bool removePendingDelete(Key key, std::shared_ptr<IPersistable> const& object);
        
        // Return in storedState the objects that have a key.
        void getStoredState(std::unordered_set<std::shared_ptr<IPersistable>>& storedState);
        void addToBuildState(std::shared_ptr<IPersistable> const& object);
        void removeFromBuildState(std::shared_ptr<IPersistable> const& object);

        std::filesystem::path _directory;
        ExecutionContext* _context
            ;
        std::shared_ptr<BTree::PersistentPagePool> _pool;
        std::shared_ptr<BTree::Forest> _forest;
        std::map<BTree::TreeIndex, BTree::StreamingTree<Key>*> _typeToTree;
        uint64_t _nextId;

        std::map<Key, std::shared_ptr<IPersistable>> _keyToObject;
        std::map<IPersistable*, Key> _objectToKey;

        std::map<Key, std::shared_ptr<IPersistable>> _keyToDeletedObject;
        std::map<IPersistable*, Key> _deletedObjectToKey;

    };

}