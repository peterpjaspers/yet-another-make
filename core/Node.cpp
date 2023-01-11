#include "Node.h"
#include "NodeSet.h"
#include "Delegates.h"
#include "ExecutionContext.h"

namespace YAM
{
	Node::Node(ExecutionContext* context, std::filesystem::path const& name)
        : _context(context)
		, _name(name)
		, _state(Node::State::Dirty)
        , _executionState(ExecutionState::Idle)
        , _suspended(false)
        , _canceling(false)
	{} 

	std::filesystem::path const& Node::name() const {
		return _name; 
	}

    ExecutionContext* Node::context() const {
        return _context;
    }

	void Node::setState(State newState) {
		if (_state != newState) {
			_state = newState;
			if (_state == Node::State::Dirty) {
				for (auto p : _parents) p->setState(Node::State::Dirty);
			}
		}
	}

	void Node::replaceParents(std::vector<Node*> const& newParents) {
		_parents.clear();
		for (auto p : newParents) addParent(p);
	}

	void Node::addParent(Node* parent) {
		auto p = _parents.insert(parent);
		if (!p.second) throw std::runtime_error("Attempt to add duplicate parent");
	}

	void Node::removeParent(Node* parent) {
		if (0 == _parents.erase(parent)) throw std::runtime_error("Attempt to remove unknown parent");
	}

    void Node::suspend() {
        if (busy()) throw std::runtime_error("Attempt to suspend while busy");
        _suspended = true;
    }

    void Node::resume() {
        _suspended = false;
        if (_executionState == ExecutionState::Suspended) {
            continueStart();
        }
    }

    void Node::start() {
        if (busy()) throw std::runtime_error("Attempt to start while busy");
        if (state() != Node::State::Dirty) throw std::runtime_error("Attempt to start while not dirty");
        _context->statistics().registerStarted(this);
        setState(Node::State::Executing);
        if (_suspended) {
            _executionState = ExecutionState::Suspended;
        }
        else {
            continueStart();
        }
    }

    void Node::continueStart() {
        if (_state == Node::State::Deleted) {
            postCompletion(State::Deleted);
        }
        else if (supportsPrerequisites()) {
            startPrerequisites();
        }
        else if (pendingStartSelf()) {
            _executionState = ExecutionState::Self;
            _context->statistics().registerSelfExecuted(this);
            startSelf();
        } else {
            postCompletion(State::Ok);
        }
    }

    void Node::startPrerequisites() {
        _executionState = ExecutionState::Prerequisites;
        _prerequisites.clear();
        getPrerequisites(_prerequisites);
        for (Node* p : _prerequisites) startPrerequisite(p);
        handlePrerequisitesCompletion();
    }

    void Node::startPrerequisite(Node* prerequisite) {
        // No subscription is made to the completed event of the prerequisites 
        // because the subscription overhead is too large.
        // Instead completion callback is done via handlePrerequisiteCompletion.
        switch (prerequisite->state()) {
        case Node::State::Dirty:
        {
            _executingPrerequisites.insert(prerequisite);
            prerequisite->start();
            break;
        }
        case Node::State::Executing:
        {
            // Multiply referenced node, already started by another
            // executor. Wait for it to complete.
            _executingPrerequisites.insert(prerequisite);
            break;
        }
        case Node::State::Ok: break;
        case Node::State::Failed: break;
        case Node::State::Canceled: break;
        case Node::State::Deleted: break;
        default:
            throw std::runtime_error("Unknown Node::State");
        }
    }

    void Node::handlePrerequisiteCompletion(Node* prerequisite) {
        // When a prerequisite completes it's recursive executor calls this
        // function for all if its parent executors. There may be parents that
        // are not part of the build scope. So ignore the callback when this
        // executor is in Idle state.
        //
        if (_executionState == ExecutionState::Prerequisites) {

            auto preqState = prerequisite->state();
            bool preqCompleted =
                preqState == Node::State::Ok
                || preqState == Node::State::Failed
                || preqState == Node::State::Canceled
                || preqState == Node::State::Deleted;
            if (!preqCompleted) {
                throw std::runtime_error("Completed prerequisite has invalid Node::State");
            }
            if (1 == _executingPrerequisites.erase(prerequisite)) {
                bool stopBuild = (preqState != Node::State::Ok); // todo: && !_node->logBook()->keepWorking();
                if (stopBuild) {
                    // At this point in the build process:
                    //  - all busy recursive executors are waiting for prerequisite
                    //    or self-executors to complete
                    //  - busy self-executors have posted operations to the thread
                    //    pool or are already executing operations in thread pool
                    //    context and are awaiting completion of these operations.
                    //    Note: a thread pool operation notifies its completion
                    //    by posting a completion operation to the main thread.
                    //    Thread pool and completion operations are member
                    //    functions of the self-executor.
                    //
                    // Stopping the build requires:
                    // 1) to suspend the thread pool
                    //    This speeds-up the cancelation process by not
                    //    starting posted operations that can not yet detect
                    //    that they must stop processing (see step 2 and 4).
                    // 2) to request all busy recursive and self-executors to
                    //    cancel the build, i.e. to take measures to stop
                    //    execution as soon as possible. I.e. for a recursive
                    //    executor to not start the self-executor.
                    //    E.g. a command node self-executor posts an operation
                    //    that runs a shell process to execute the command
                    //    script. This operation blocks until the shell exits. 
                    //    To expedite cancelation the command node executor sets
                    //    a cancel flag, kills the shell and awaits completion
                    //    notification. Also see step 4.
                    // 3) to resume the thread pool
                    // 4) an operation running in thread pool to (frequently)
                    //    check the cancel flag and to stop processing if it is
                    //    set. E.g. in step 2 a shell process is killed causing
                    //    the thread pool operation to unblock. The operation
                    //    checks the cancel flag, and if set, stops further
                    //    processing and posts a completion operation with status
                    //    canceled to the main thread.
                    //
                    cancel();
                    // TODO: how to notify all other executors that build must be stopped ?
                    //mainDispatcher().post((_node->LogBook.RaiseStopRequested));
                }

                // TODO: can prerequisites indeed contain duplicates? Should
                // that be disallowed?
                //
                // Example: a source file node can be both registered as an input 
                // file and as a prerequisite. In that case this callback can be 
                // called more than once from the same node. However, registering
                // a source file as prerequisite make little sense.
                handlePrerequisitesCompletion();
            }
        }
    }

    void Node::handlePrerequisitesCompletion() {
        bool allCompleted = _executingPrerequisites.size() == 0;
        if (allCompleted) {
            if (_canceling) {
                postCompletion(State::Canceled);
            }
            else if (!allPrerequisitesAreOk()) {
                postCompletion(State::Failed);
            }
            else if (pendingStartSelf()) {
                _executionState = ExecutionState::Self;
                _context->statistics().registerSelfExecuted(this);
                startSelf();
            }
            else {
                postCompletion(State::Ok);
            }
        }
    }

    bool Node::allPrerequisitesAreOk() const {
        bool allOk = true;
        for (std::size_t i = 0; i < _prerequisites.size() && allOk; ++i) {
            allOk = _prerequisites[i]->state() == Node::State::Ok;
        }
        return allOk;
    }

    void Node::postCompletion(Node::State newState) {
        auto d = Delegate<void>::CreateLambda([this, newState]()
            {
                notifyCompletion(newState);
            });
        _context->mainThreadQueue().push(std::move(d));
    }

    void Node::notifyCompletion(Node::State newState) {
        _executionState = ExecutionState::Idle;
        _canceling = false;
        _prerequisites.clear();
        _executingPrerequisites.clear();

        setState(newState);

        for (auto p : _parents) p->handlePrerequisiteCompletion(this);
        _completor.Broadcast(this);
    }

    // Pre: busy()
    // Request cancelation of execution.
    // Return immediately, do not block caller until execution has completed.
    // Notify execution completion as specified by start() function. 
    bool Node::busy() const {
        return _state == State::Executing;
    }

    void Node::cancel() {
        if (!busy()) {
            throw std::runtime_error("Attempt to cancel while not busy");
        }
        if (!_canceling) {
            _canceling = true;
            switch (_executionState) {
            case ExecutionState::Idle:
                break;
            case ExecutionState::Suspended:
                postCompletion(State::Canceled);
                break;
            case ExecutionState::Prerequisites:
                for (auto preq : _executingPrerequisites) {
                    preq->cancel();
                }
                break;
            case ExecutionState::Self:
                cancelSelf();
            default:
                throw std::runtime_error("invalid _executionState");
            }
        }
    }
}