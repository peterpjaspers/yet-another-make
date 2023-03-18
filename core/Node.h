#pragma once

#include "Delegates.h"
#include "IPersistable.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <vector>
#include <unordered_set>
#include <stdexcept>
#include <atomic>
#include <initializer_list>

namespace YAM
{
    class ExecutionContext;

    // Node is the base class of nodes in the YAM build graph. It has interfaces
    // to execute a node and to access the inputs used and the outputs produced
    // by the node execution. 
    // Typical node subtypes for a SW build application like YAM are:
    //     - Command node
    //       Execution produces one or more output files. Execution may read
    //         source files and/or output files produced by other nodes.
    //         E.g. a C++ compile node produces an object file and reads a cpp and
    //         header files.
    //     - Generated file node
    //       An output file of a command node. 
    //         Execution (re-)computes hashes of the file content. 
    //     - Source file node
    //       Associated with a source file.
    //         Execution (re-)computes hashes of the file content.
    //
    // A node's name takes the form of a file path. For file nodes this path
    // indeed references a file. In general however this need not be the case.
    // E.g. a command node may be given a name like 'name-of-first-output\_cmd'
    //
    // The Node class supports a 3-stage execution of a node. This 3-stage
    // execution is started by the start() member function. 
    //    stage 1: Execute the prerequisites of the node.
    //             Prerequisite nodes produce results that are needed as input
    //               by stage 2. E.g. the prerequisites of a link command node are
    //             compilation nodes that produce the object files to be linked.
    //             The stage 1 execution results in a depth-first traversal of
    //               the prerequisites node tree.
    //    stage 2: Self-execution of the node.
    //               In this stage the node can perform node-specific computation.
    //               E.g. link C++ object files into a library.
    //               This stage is started by the startSelf() virtual member function.
    //    stage 3: Execute the postrequisites of the node.
    //             Postrequisite nodes are typically produced by stage 2.
    //             E.g. the execution of a directory node must create a node 
    //             hierachy that reflects the content of a directory tree in the
    //               filesystem. Further it must compute for each directory and file 
    //             node a hash of their content. 
    //             The directory node has no prerequisites.
    //             The directory node's self-execution iterates a directory and
    //             creates for each directory entry a file or directory node.
    //               The postrequisites are the file and directory nodes created in 
    //             stage 2. The stage 3 execution results in a breadth-first 
    //             traversal of the filesystem directory tree.
    //
    // Name convention: node execution is synonym for 3-stage node execution.
    //
    class __declspec(dllexport) Node : public IPersistable
    {
    public:
        enum class State {
            Dirty = 1,        // needs execution
            Executing = 2,    // execution is in progress
            Ok = 3,            // execution has succeeded
            Failed = 4,         // execution has failed
            Canceled = 5,   // execution was canceled
            Deleted = 6        // node is deleted, to be removed from node graph

            // A dirty node is a node whose output filess may no longer be valid. 
            // E.g. because the output files have been tampered with or because 
            // input files have been modified. A node must be marked Dirty when:
            //  - it is created,
            //    - its self-execution logic is modified,
            //    e.g. the shell script of a command node is modified,
            //  - a prerequisite is marked Dirty.
            // 
            // Nodes without prerequisites are reset to Dirty depending on their
            // type. E.g. for file and directory nodes:
            // At start of alpha-build all file and directory nodes are set Dirty.
            // At start of beta-build only files and directories reported by the 
            // filesystem to have been write-accessed since previous build are set
            // dirty.
            //
            // Failed and Canceled states become meaningless when starting a new
            // build and must therefore be reset to Dirty at start of build.
        };

        Node(); // needed for deserialization
        Node(ExecutionContext* context, std::filesystem::path const & name);
        virtual ~Node();

        ExecutionContext* context() const;
        std::filesystem::path const& name() const;

        State state() const { return _state; }
        virtual void setState(State newState);

        // Return whether this node supports prerequisites.
        // This is a class property.
        //
        // Prerequisites are nodes that are executed before this node 
        // starts self-execution.
        virtual bool supportsPrerequisites() const { return false; }

        // Pre: supportsPrerequisites()
        // Append prerequisite nodes to 'prerequisites'.
        // The prerequisites vector contains one or more of:
        //        - (command) nodes that produce output nodes. 
        //          Output nodes are only allowed to be used as inputs by this
        //          node when the nodes that produce these outputs are in
        //          prerequisites.
        //        - source nodes used as inputs by last execution of this node.
        //        - output nodes of this node.
        // 
        // Source/output nodes are typically, but not necessarily, file nodes.
        virtual void getPrerequisites(std::vector<std::shared_ptr<Node>>& prerequisites) const {};

        // Return whether this node supports postrequisites.
        // This is a class property.
        //
        // Postrequisites are nodes that are executed after this node 
        // completed its self-execution.
        virtual bool supportsPostrequisites() const { return false; }

        // Pre: supportsPostrequisites()
        // Append postrequisite nodes to 'postrequisites'.
        virtual void getPostrequisites(std::vector<std::shared_ptr<Node>>& postrequisites) const { return; }

        // A preParent node is a node that has this node as a prerequisite. This node 
        // propagates its Dirty state to all its preParents. 
        // E.g. a C++ header file that is set Dirty will propagate the Dirty states to
        // all C++ compilation commands that have the header file as input node.
        // The compilation command on its turn propagates the Dirty state to all link
        // command nodes that have the object file as input.
        // The preParent node must:
        //    - add itself to the preParents of this node when it uses this node as 
        //      prerequisite.
        //    - remove itself from the preParents of this node when it stops using 
        //     this node as prerequisite.
        // The preParent relation causes a cyclic dependency between preParent and this
        // node. Therefore raw pointer to preParent is used iso shared_ptr. This is safe 
        // as long as preParent adheres to the add/remove protocol. 
        std::unordered_set<Node*> const& preParents() const { return _preParents; }
        void addPreParent(Node* parent);
        void removePreParent(Node* parent);

        // A postParent node is a node that has this node as a postrequisite. 
        // This node does NOT propagate its Dirty state to its postParents.
        // Why not? Example:
        // A filesystem directory A contains file A\F and directory A\D.
        // SourceDirectoryNode A creates file node A\F and directory node A\D.
        // These sub nodes are postrequisites of A: they are executed after 
        // self-execution of SourceDirectoryNode A has created these subnodes.
        // A is therefore postParent of both A\F and A\D.
        // 
        // The postParent node must:
        //    - add itself to the postParents of this node when it uses this node as 
        //      postrequisite.
        //    - remove itself from the postParents of this node when it stops using 
        //     this node as postrequisite.
        // The postParent relation causes a cyclic dependency between postParent and this
        // node. Therefore raw pointer to postParent is used iso shared_ptr. This is safe 
        // as long as postParent adheres to the add/remove protocol. 
        // 
        // TODO: typically only one postParent will exist. Should we constrain to 1
        // postParent?
        std::unordered_set<Node*> const& postParents() const { return _postParents; }
        void addPostParent(Node* parent);
        void removePostParent(Node* parent);
        
        //
        // Start of interfaces that access inputs and outputs of node execution.
        //
        
        // Return whether this node supports having output nodes.
        // This is a class property.
        virtual bool supportsOutputs() const { return false; }

        // Pre: supportsOutputs()
        // Append the output nodes to 'outputs'.
        // Note: outputs are typically, but not necessarily, output files nodes.
        virtual void getOutputs(std::vector<std::shared_ptr<Node>>& outputs) const {};

        // Return whether this node supports input nodes.
        // This is a class property. 
        virtual bool supportsInputs() const { return false; }

        // Pre: supportsInputs()
        // Append the inputs nodes to 'inputs'.
        // Note: inputs are typically, but not necessarily, source files and/or
        // output files produced by prerequisite command nodes.
        virtual void getInputs(std::vector<std::shared_ptr<Node>>& inputs) const {};

        //
        // End of interfaces that access inputs and outputs of node execution.
        // Start of Node execution interfaces
        //

        // Pre: state() == Node::State::Dirty
        // Start asynchronous 3-stage execution:
        //    1: prerequisites.where(Dirty).each(start)
        //      2: on prerequisites completion: if (pendingStartSelf()) startSelf()
        //     3a: on startSelf completion: postrequisites.where(Dirty).each(start)
        //     3b: on postrequisites completion: completor().broadcast(this)
        // All start() and startSelf() calls are made in current thread. 
        // completor().broadcast(this) call is made in context()->mainThread(). 
        void start();

        // Return delegate to which clients can add callbacks that will be
        // executed when execution completes. See start(). 
        MulticastDelegate<Node*>& completor() { return _completor; }

        // Return whether execution (including execution cancelation) is in 
        // progress. Set busy() to false in main thread context just before
        // broadcast of completor().
        // Rationale: avoid race-condition where main thread code calls
        // cancel() while execution just completed but completion callback
        // was not yet processed by main thread.
        bool busy() const;

        // Pre: busy()
        // Request cancelation of execution.
        // Return immediately, do not block caller until execution has completed.
        // Notify execution completion as specified by start() function. 
        void cancel();

        // Pre: !busy()
        // Suspend execution.
        // While suspended: execution waits for node to be resumed() before
        // continuing execution.
        //
        // Rationale: nodes (like command nodes) may be updated due to changes 
        // in build files. Build file processing protocol:
        //        1) For each modified build file:
        //           nodes: the set of nodes previously produced from build file
        //           previousNodes = nodes;
        //           nodes = ();
        //           previousNodess.each(suspend())
        //        2) start execution of nodes in build scope
        //        3) for each modified build file:
        //                a) nodes = create/update nodes from build file
        //                b) deletedNodes = previousNodes - nodes
        //                   Set nodes in deletedNodes to NodeState::Deleted
        //                c) previousNodes.each(resume())
        //
        // Note: nodes not suspended in step 1) execute concurrently with step 3). 
        // Suspended nodes will resume in step 3c).
        void suspend();
        
        bool suspended() const;

        // Pre: true
        // When busy(): resume execution
        void resume();
        //
        // End of Node execution interfaces
        //

        // Inherited from IPersistable
        void modified(bool newValue) override;
        bool modified() const override;

        // Inherited from IStreamer (via IPersistable)
        void stream(IStreamer* streamer) override;
        // Inherited from IPersistable
        void restore(void* context) override;

    protected:
        ExecutionContext* _context;
        std::filesystem::path _name;
        std::atomic<bool> _canceling;

        // Pre: state() == State::Executing && all prerequisites are in State::Ok.
        // Return true when self-execution is needed. 
        // 
        // Self-execution is needed when the node was never executed or when 
        // there is reason to believe that previously computed outputs are no
        // longer valid, typically because inputs (may) have changed.
        // A node that has no self-execution implements this as: return false. This is
        // the default implementation. 
        // 
        // Example:
        // Assume all nodes are in State::Ok (i.e. have been executed successfully)
        // and consider the following three nodes:
        //    L: command node that links the object file produced by node C into a library
        //        C: command node that compiles C++ source file F
        //            F: C++ source file node
        // 
        // The user now edits the C++ file:
        // The filesystem notifies YAM that the C++ file has been written.
        // YAM adds the file to the list of write-accessed files.
        // 
        // The user then invokes YAM to build the library: 
        // YAM marks the C++ source file node Dirty. The Dirty marking is
        // propagated recursively to the compilation and link command nodes.
        // 
        // YAM starts execution of link node L (descending down-to the leaf nodes)
        // Because the link node is Dirty YAM recursively executes the link
        // node which recursively executes its Dirty prerequisite compile node C 
        // node which recursively executes its Dirty prerequisite file node F.
        // 
        // Self-execution (ascending up-to the link node):
        // The file node (re-)computes the hash of the C++ file.
        // On file node completion the compile node detects that the C++ hash 
        // has changed. This invalidates the output object file, hence it is 
        // pendingStartSelf(). 
        // 
        // Assume that the C++ changes were only in comments. Comments do not
        // affect object file content and therefore the hash of the object 
        // file will not change, hence the link node is not pendingStartSelf().
        // This completes the build.
        // 
        // Note: YAM is also capable of preventing re-compilation by using 
        // aspect hashes. For C++ files an aspect hash exists that excludes 
        // the comments from being hashed.
        virtual bool pendingStartSelf() const { return false; }

        // Pre:pendingStartSelf().
        // Delegate self-execution of the node to context()->threadPool().
        // On completion: call notifyCompletion() in context()->mainThread().
        virtual void startSelf() { postCompletion(Node::State::Failed); }

        // Called when self-execution is busy and cancel() is called.
        virtual void cancelSelf() { return; }

        // Push handleSelfCompletion(newState) to mainThreadQueue()
        void postSelfCompletion(Node::State newState);

        virtual void handleSelfCompletion(Node::State newState);

        // Push notifyCompletion(newState) to mainThreadQueue()
        void postCompletion(Node::State newState);

        void notifyCompletion(Node::State newState);

    private:
        State _state;
        std::unordered_set<Node*> _preParents;
        std::unordered_set<Node*> _postParents;

        // Node execution members
        //
        void continueStart();
        void startPrerequisites();
        void startPrerequisite(Node* prerequisite);
        void handlePrerequisiteCompletion(Node* prerequisite);
        void handlePrerequisitesCompletion();
        bool allPrerequisitesAreOk() const;

        void startPostrequisites();
        void startPostrequisite(Node* postrequisite);
        void handlePostrequisiteCompletion(Node* postrequisite);
        void handlePostrequisitesCompletion();
        bool allPostrequisitesAreOk() const;

        enum class ExecutionState
        {
            Idle,           // waiting for start()
            Suspended,     // started while suspended, waiting for node to be resumed
            Prerequisites, // waiting for prerequisites to finish execution
            Self,           // waiting for self-execution to finish
            Postrequisites // waiting for postrequisites to finish execution
        };
        ExecutionState _executionState;
        bool _suspended;
        bool _modified;
        // prerequisite nodes, captured at start-resume time.
        std::vector<std::shared_ptr<Node>> _prerequisites;
        // nodes in _prerequisites that are executing  
        std::unordered_set<Node*> _executingPrerequisites;

        // postrequisite nodes, captured at completion of self-execution.
        std::vector<std::shared_ptr<Node>> _postrequisites;
        // nodes in _postrequisites that are executing  
        std::unordered_set<Node*> _executingPostrequisites;

        MulticastDelegate<Node*> _completor;
    };
}

