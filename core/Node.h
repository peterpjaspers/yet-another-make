
#pragma once

#include "Delegates.h"
#include "IPersistable.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <vector>
#include <map>
#include <unordered_set>
#include <stdexcept>
#include <atomic>
#include <initializer_list>

#ifdef _DEBUG
#define ASSERT_MAIN_THREAD(contextPtr) (contextPtr)->assertMainThread()
#else
#define ASSERT_MAIN_THREAD(contextPtr)
#endif

namespace YAM
{
    class ExecutionContext;
    class Node;
    class FileRepositoryNode;

    class StateObserver {
    public:
        // Called when node state changes from Executing to Ok, Failed
        // or Canceled
        virtual void handleCompletionOf(Node* observedNode) = 0;

        // Called when node state changes to Dirty.
        virtual void handleDirtyOf(Node* observedNode) = 0;
    };

    // Node is the base class of nodes in the YAM build graph. It provides an
    // interface to execute a node. Semantics of node execution are determined 
    // by derived classes.
    //
    // A node is not MT-safe: all member functions except postCompletion must
    // be called from ExecutionContext::mainThread(). Applications must access
    // node state and ExecutionContext::nodes() from mainThread only.
    //  
    class __declspec(dllexport) Node : public IPersistable, protected StateObserver
    {
    public:
        enum class State {
            Dirty = 1,      // pending execution
            Executing = 2,  // execution is in progress
            Ok = 3,         // last execution succeeded
            Failed = 4,     // last execution failed
            Canceled = 5,   // last execution was canceled
            Deleted = 6,    // node is pending destruction
        };

        // Implementation of std::less allows nodes to be stored in a std::set
        // in node->name() order.
        struct CompareName {
            constexpr bool operator()(const std::shared_ptr<Node>& lhs, const std::shared_ptr<Node>& rhs) const {
                return lhs->name() < rhs->name();
            }
        };

        Node(); // needed for deserialization
        Node(ExecutionContext* context, std::filesystem::path const & name);
        virtual ~Node();

        ExecutionContext* context() const { return _context; }

        // Return name of this name. name() format is: <repoName>\<path>
        // where <repoName> matches one of the names in context()->repositories().
        std::filesystem::path const& name() const { return _name; }

        virtual std::string className() const { return typeid(*this).name(); }

        // Return the repository that contains this node.
        std::shared_ptr<FileRepositoryNode> const& repository() const;

        // Return the absolute path name of the node, i.e. return name() in which
        // <repoName> is replace by the repository absolute root directory path.
        std::filesystem::path absolutePath() const;

        State state() const { return _state; }

        // Pre: state() != Node::State::Deleted
        virtual void setState(State newState);

        // Start asynchronous execution.
        // Sub-classes must override this function as follows:
        //    void start() override {
        //       Node::start();
        //       sub-class specific execution logic
        //    }
        // On completion the sub-class must call notifyCompletion when in
        // main thread or call postCompletion when not in main thread.
        //
        // Pre: state() == Node::State::Dirty
        virtual void start();

        // Return delegate to which clients can add callbacks that will be
        // executed when execution of this node completes.
        MulticastDelegate<Node*>& completor() { return _completor; }

        // The MulticastDelegate overhead of adding/removing/calling callbacks
        // is considerable. The *Observer() interfaces provide a faster, but
        // less flexible, alternative for the completor() interface for the 
        // special case where:
        //    - observer is a StateObserver 
        //    - an node X notifies its observers of a change in X->state() by
        //      calling: for (obs : X->observers()) obs->handleStateChangeOf(X)
        // 
        void addObserver(StateObserver* observer);
        void removeObserver(StateObserver* observer);
        std::unordered_set<StateObserver*> const& observers() const {
            return _observers;
        }

        // Cancel node execution.
        // Cancelation is asynchronous: completion as specified by start() 
        // function. 
        // Sub-classes must override this function as follows:
        //    void cancel() override {
        //       Node::cancel();
        //       sub-class specific cancel logic
        //    }
        virtual void cancel();

        // Return whether cancelation is in progress.
        bool canceling() const { return _canceling; }

        // Node execution may produce outputs whose content depends on inputs.
        virtual void getOutputs(std::vector<std::shared_ptr<Node>>& outputs) const {}
        virtual void getInputs(std::vector<std::shared_ptr<Node>>& inputs) const {}

        // Inherited from IPersistable
        void modified(bool newValue) override;
        bool modified() const override;
        bool deleted() const override;
        // Pre: state() == Node::State::Deleted.
        // Post: state() == Node::State::Dirty.
        // State changed will not be notified to subscribers.
        void undelete() override;
        std::string describeName() const override { return _name.string(); }
        std::string describeType() const override { return className(); }

        // Inherited from IStreamer (via IPersistable)
        void stream(IStreamer* streamer) override;
        // Inherited from IPersistable
        void prepareDeserialize() override;
        bool restore(void* context, std::unordered_set<IPersistable const*>& restored) override;

    protected:
        // Called when node state is set to Node::State::Deleted
        // Subclasses must release all references to other nodes,
        // and remove themselves as observer of other nodes.
        virtual void cleanup() {}

        // Implements StateObserver::handleCompletionOf
        // Overrides must be implemented like:
        //      sub-class specific logic
        //      Node::handleCompletionOf(observedNode)
        void handleCompletionOf(Node* observedNode) override;
        // Set state Dirty iff not Deleted
        void handleDirtyOf(Node* observedNode) override;

        // Start asynchronous execution of given set of 'nodes'.
        // On completion call callback.Execute(state), where state
        // is one of:
        //    - Ok when all nodes executed successfully
        //    - Failed when at least one node execution did not succeed
        //    - Canceled when execution of this node was canceled
        //
        // Pre: each node in nodes is observed by this node, i.e. 
        //      for (n : nodes) n->observers().contains(this)
        // Pre: state() == Node::State::Executing
        //
        // Note: this is a convenience interface for subclasses whose execution
        // logic needs execution of a collection of nodes. 
        // Note: caller is responsible to keep 'nodes' in existence during
        // execution.
        //
        template <class TNode> 
        void startNodes(
            std::vector<std::shared_ptr<TNode>> const& nodes,
            Delegate<void, Node::State> const& callback
        ) {
            std::vector<Node*> rawNodes;
            for (auto const& n : nodes) rawNodes.push_back(n.get());
            startNodes(rawNodes, callback);
        }

        void startNodes(
            std::vector<Node*> const& nodes,
            Delegate<void, Node::State> const& callback);

        // Push notifyCompletion(newState) to context()->mainThreadQueue()
        // To be called by subclass to notify execution completion from 
        // any thread.
        void postCompletion(Node::State newState);

        // To be called by subclass to notify execution completion from main
        // thread. This function:
        //    - sets node state to newState
        //    - notifies observers()
        //    - executes completor()
        void notifyCompletion(Node::State newState);

    private:
        void startNode(Node* node);
        void _handleNodesCompletion();

        ExecutionContext* _context;
        std::filesystem::path _name;
        State _state;
        std::atomic<bool> _canceling;

        // As requested by startNodes(..),
        Delegate<void, Node::State> _callback;
        std::unordered_set<Node*> _nodesToExecute;
        // The size of the subset of _nodesToExecute that are executing.
        std::size_t _nExecutingNodes;
#ifdef _DEBUG
        // nodes in _nodesToExecute that are executing  
        // _nExecutingNodes = _executingNodes.size()
        std::unordered_set<Node*> _executingNodes;
#endif

        MulticastDelegate<Node*> _completor;
        bool _notifyingObservers;
        std::unordered_set<StateObserver*> _observers;
        // Observers that were added/removed while _notifyingObservers
        std::vector<std::pair<StateObserver*, bool>> _addedAndRemovedObservers;

        bool _modified;
    };
}

