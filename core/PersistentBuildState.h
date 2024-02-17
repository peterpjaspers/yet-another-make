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
        // If startRepoWatching: start watching repositories after retrieval.
        PersistentBuildState(
            std::filesystem::path const& directory,
            ExecutionContext* context,
            bool startRepoWatching);

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

        void logState(ILogBook &logBook);

    private:
        // retrieve for use by SharedPersistableReader
        std::shared_ptr<IPersistable> retrieve(Key key);
        // store for use by SharedPersistableWriter
        Key store(std::shared_ptr<IPersistable> const& object);

        void reset();
        void retrieveAll();
        void retrieveKey(Key key);
        void retrieveKey(Key key, BTree::ValueReader<Key>& reader);

        Key bindToKey(std::shared_ptr<IPersistable> const& object);
        Key allocateKey(IPersistable* object);
        void store(std::shared_ptr<IPersistable> const& object, bool replace);

        void remove(Key key, std::shared_ptr<IPersistable> const& object);

        // If recoverForest: recover forest to last commit.
        // Restore build state to state at last commit.
        void rollback(bool recoverForest); 
        
        // Return in storedState the objects that have a key.
        void getStoredState(std::unordered_set<std::shared_ptr<IPersistable>>& storedState);
        void addToBuildState(std::shared_ptr<IPersistable> const& object);
        void removeFromBuildState(std::shared_ptr<IPersistable> const& object);

        std::filesystem::path _directory;
        ExecutionContext* _context;
        bool _startRepoWatching;
        std::shared_ptr<BTree::PersistentPagePool> _pool;
        std::shared_ptr<BTree::Forest> _forest;
        std::map<BTree::TreeIndex, BTree::StreamingTree<Key>*> _typeToTree;
        uint64_t _nextId;
        std::map<Key, std::shared_ptr<IPersistable>> _keyToObject;
        std::map<IPersistable*, Key> _objectToKey;
        std::unordered_set<std::shared_ptr<IPersistable>> _toInsert;
        std::unordered_set<std::shared_ptr<IPersistable>> _toReplace;
        std::unordered_set<std::shared_ptr<IPersistable>> _toRemove;
        std::map<IPersistable*, Key> _toRemoveKeys;
    };

}