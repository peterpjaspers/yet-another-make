#pragma once

#include "Delegates.h"

#include <filesystem>
#include <functional>
#include <vector>
#include <unordered_set>
#include <stdexcept>
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
	//	     source files and/or output files produced by other nodes.
	//		 E.g. a C++ compile node produces an object file and reads a cpp and
	//		 include files.
	//     - Output file node (or derived file node)
	//       Associated with an output file of a command node. 
	//		 Execution (re-)computes hashes of the file content. 
	//     - Source file node
	//       Associated with a source file.
	//		 Execution (re-)computes hashes of the file content.
	//
	// A node's name takes the form of a file path. For file nodes this path
	// indeed references a file. In general however this need not be the case.
	// E.g. a command node may be given a name like 'name-of-first-output\_cmd'
	class __declspec(dllexport) Node
	{
	public:
		enum class State {
			Dirty = 1,	    // needs recursive execution
			Executing = 2,	// recursive execution is in progress
			Ok = 3,	        // recursive execution has succeeded
			Failed = 4,	 	// recursive execution has failed
			Canceled = 5,   // recursive execution was canceled
			Deleted = 6		// node is deleted, to be garbage collected

			// A dirty node is a node whose output may no longer be valid. E.g.
			// because the output has been tampered with or because input files
			// may have been modified. A node must be marked Dirty when:
			//  - it is created,
			//	- its execution logic is modified,
			//    e.g. the shell script of a command node is modified,
			//  - a prerequisite is marked Dirty.
			// 
			// Nodes without prerequisites are reset to Dirty depending on their
			// type. E.g. for a file node:
			// At start of alpha-build all file nodes are set Dirty.
			// At start of beta-build only files reported by the filesystem to
			// have been write-accessed since previous build are set dirty.
			//
			// Failed and Canceled states become meaningless when starting a new
			// build and must therefore be reset to Dirty at start of build.
		};

		Node(ExecutionContext* context, std::filesystem::path const & name);

		ExecutionContext* context() const;
		std::filesystem::path const& name() const;

		State state() const { return _state; }
		void setState(State newState);

		// Return whether this node supports prerequisites.
		// This is a class property.
		//
		// Prerequisites are nodes that need to execute before this node can
		// execute.
		// The prerequisites vector contains one or more of:
		//		- (command) nodes that produce output nodes. 
		//	      Output nodes are only allowed to be used as inputs by this
		//		  node when the nodes that produce these outputs are in
		//		  prerequisites.
		//		- source nodes used as inputs by last execution of this node.
		//		- output nodes of this node.
		// 
		// Source/output nodes are typically, but not necessarily, file nodes.
		virtual bool supportsPrerequisites() const = 0;

		// Pre: supportsPrerequisites()
		// Append prerequisite nodes to 'prerequisites'.
		virtual void getPrerequisites(std::vector<Node*>& prerequisites) const = 0;

		// Return parent nodes.
		// A parent node has this node as a prerequisite. 
		std::unordered_set<Node*> const& parents() const { return _parents; }
		
		//
		// Start of interfaces that access inputs and outputs of node execution.
		//
		
		// Return whether this node supports having output nodes.
		// This is a class property.
		virtual bool supportsOutputs() const = 0;

		// Pre: supportsOutputs()
        // Append the output nodes in 'outputs'.
		// Note: outputs are typically, but not necessarily, output files nodes.
		virtual void getOutputs(std::vector<Node*>& outputs) const = 0;

		// Return whether this node supports input nodes.
		// This is a class property. 
		virtual bool supportsInputs() const = 0;

		// Pre: supportsInputs()
		// Append the inputs nodes in 'inputs'.
		// Note: inputs are typically, but not necessarily, source files and/or
		// output files produced by prerequisite command nodes.
		virtual void getInputs(std::vector<Node*>& inputs) const = 0;

		//
		// End of interfaces that access inputs and outputs of node execution.
		// Start of Node execution interfaces
		//

		// Pre: state() == Node::State::Dirty
		// Start asynchronous execution of prerequisites and self:
		//		- prerequisites.each(start)
		//		- on prerequisites completion: if (pendingStartSelf()) startSelf()
		//		- on self completion: completor().broadcast(this)
		// All three steps will be done in main thread. Actual self-execution
		// will be done in thread pool. 
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
		//		1) For each modified build file:
		//		   nodes: the set of nodes previously produced from build file
		//		   previousNodes = nodes;
		//		   nodes = ();
		//		   previousNodess.each(suspend())
		//		2) start execution of nodes in build scope
		//		3) for each modified build file:
		//				a) nodes = create/update nodes from build file
		//				b) deletedNodes = previousNodes - nodes
		//				   Set nodes in deletedNodes to NodeState::Deleted
		//				c) previousNodes.each(resume())
		//
		// Note: nodes not suspended in step 1) execute concurrently with step 3). 
		// Suspended nodes will resume in step 3c).
		void suspend();

		// Pre: true
		// When busy(): resume execution
		void resume();
		//
		// End of Node execution interfaces
		//

		void replaceParents(std::vector<Node*> const& newParents);
		void addParent(Node* parent);
		void removeParent(Node* parent);

	protected:
		ExecutionContext* _context;

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

		// Pre:pendingStartSelf(), current thread is main thread.
		// Start self-execution of this node. E.g. for a compile command node
		// start the compilation. Perform as much processing as possible by
		// posting operations to threadPoolQueue.
		// On completion: push notifyCompletion() to mainThreadQueue.
		virtual void startSelf() { throw std::runtime_error("not implemented"); }

		// Pre:current thread is main thread.
		// Called when self-execution is busy and cancel() is called.
		virtual void cancelSelf() { return; }

		// Push notifyCompletion(newState) to mainThreadQueue()
		void postCompletion(Node::State newState);

		void notifyCompletion(Node::State newState);

	private:
		std::filesystem::path _name;
		State _state;
		std::unordered_set<Node*> _parents;

		// Node execution members
		//
		void continueStart();
		void startPrerequisites();
		void startPrerequisite(Node* prerequisite);
		void handlePrerequisiteCompletion(Node* prerequisite);
		void handlePrerequisitesCompletion();
		bool allPrerequisitesAreOk() const;

		enum class ExecutionState
		{
			Idle,		   // waiting for start()
			Suspended,     // started while suspended, waiting for node to be resumed
			Prerequisites, // waiting for prerequisites to finish execution
			Self		   // waiting for self-execution to finish
		};
		ExecutionState _executionState;
		bool _suspended;
		// node prerequisites, captured at start-resume time.
		std::vector<Node*> _prerequisites; 
		// nodes in _prerequisites that are executing  
		std::unordered_set<Node*> _executingPrerequisites;
		bool _canceling;
		MulticastDelegate<Node*> _completor;
	};
}

