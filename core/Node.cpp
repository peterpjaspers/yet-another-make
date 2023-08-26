#include "Node.h"
#include "NodeSet.h"
#include "Delegates.h"
#include "ExecutionContext.h"
#include "IStreamer.h"

#include <iostream>

#ifdef _DEBUG
#define ASSERT_MAIN_THREAD(contextPtr) (contextPtr)->assertMainThread()
#else
#define ASSERT_MAIN_THREAD(contextPtr)
#endif

namespace {
    using namespace YAM;

    bool allNodesAreOk(std::vector<std::shared_ptr<Node>> const& nodes)
    {
        for (auto const& n : nodes) {
            if (n->state() != Node::State::Ok) return false;
        }
        return true;
    }
}

namespace YAM
{
    Node::Node()
        : _context(nullptr)
        , _canceling(false)
        , _state(Node::State::Dirty)
        , _executionState(ExecutionState::Idle)
        , _suspended(false)
        , _modified(false)
        , _nExecutingPrerequisites(0)
        , _nExecutingPreCommitNodes(0)
        , _nExecutingPostrequisites(0)
    {}

    Node::Node(ExecutionContext* context, std::filesystem::path const& name)
        : _context(context)
        , _name(name)
        , _canceling(false)
        , _state(Node::State::Dirty)
        , _executionState(ExecutionState::Idle)
        , _suspended(false)
        , _modified(true)
        , _nExecutingPrerequisites(0)
        , _nExecutingPreCommitNodes(0)
        , _nExecutingPostrequisites(0)
    {} 

    Node::~Node() {
    }

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
                for (auto p : _dependants) p->setState(Node::State::Dirty);
            }
        }
    }

    void Node::addDependant(Node* dependant) {
        auto p = _dependants.insert(dependant);
        if (!p.second) throw std::runtime_error("Attempt to add duplicate dependant");
    }

    void Node::removeDependant(Node* dependant) {
        if (0 == _dependants.erase(dependant)) throw std::runtime_error("Attempt to remove unknown dependant");
    }

    void Node::addPostParent(Node* parent) {
        auto p = _postParents.insert(parent);
        if (!p.second) throw std::runtime_error("Attempt to add duplicate postParent");
    }

    void Node::removePostParent(Node* parent) {
        if (0 == _postParents.erase(parent)) throw std::runtime_error("Attempt to remove unknown postParent");
    }

    void Node::suspend() {
        if (busy()) throw std::runtime_error("Attempt to suspend while busy");
        _suspended = true;
    }

    bool Node::suspended() const {
        return _suspended;
    }

    void Node::resume() {
        _suspended = false;
        if (_executionState == ExecutionState::Suspended) {
            continueStart();
        }
    }

     void Node::start() {
        ASSERT_MAIN_THREAD(_context);
        if (busy()) throw std::runtime_error("Attempt to start while busy");
        if (state() != Node::State::Dirty) throw std::runtime_error("Attempt to start while not dirty");
        _context->statistics().registerStarted(this);
        setState(Node::State::Executing);
        if (_suspended) {
            _executionState = ExecutionState::Suspended;
        } else {
            continueStart();
        }
    }

    void Node::continueStart() {
        if (_state == Node::State::Deleted) {
            postCompletion(Node::State::Deleted);
        } else if (supportsPrerequisites()) {
            startPrerequisites();
        } else if (pendingStartSelf()) {
            _executionState = ExecutionState::Self;
            startSelf();
        }  else if (supportsPostrequisites()) {
            startPostrequisites();
        } else {
            postCompletion(State::Ok);
        }
    }

    void Node::startPrerequisites() {
        _executionState = ExecutionState::Prerequisites;
        getPrerequisites(_prerequisites);
        for (auto const& p : _prerequisites) startPrerequisite(p.get());
        if (_nExecutingPrerequisites == 0) handlePrerequisitesCompletion();
    }

    void Node::startPrerequisite(Node* prerequisite) {
        // No subscription is made to the completed event of the prerequisites 
        // because the subscription overhead is too large.
        // Instead completion callback is done via handlePrerequisiteCompletion.
        switch (prerequisite->state()) {
        case Node::State::Dirty:
        {
            _nExecutingPrerequisites += 1;
#ifdef _DEBUG
            _executingPrerequisites.insert(prerequisite);
#endif
            prerequisite->start();
            break;
        }
        case Node::State::Executing:
        {
            _nExecutingPrerequisites += 1;
#ifdef _DEBUG
            // Multiply referenced node, already started by another
            // executor. Wait for it to complete.
            _executingPrerequisites.insert(prerequisite);
#endif
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

    void Node::handlePrerequisiteCompletion(Node* node) {
        // When a node completes it calls this function for ALL if its 
        // dependants. There may be dependants that are not part of the build 
        // scope and are not executing.
        if (_executionState == ExecutionState::Prerequisites) {
#ifdef _DEBUG
            if (0 == _executingPrerequisites.erase(node)) {
                throw std::exception("callback from unknown prerequisite");
            }
#endif
            if (0 == _nExecutingPrerequisites) {
                throw std::exception("Invalid _nExecutingPrerequisites");
            }
            _nExecutingPrerequisites -= 1;

            auto preqState = node->state();
            bool preqCompleted =
                preqState == Node::State::Ok
                || preqState == Node::State::Failed
                || preqState == Node::State::Canceled
                || preqState == Node::State::Deleted;
            if (!preqCompleted) {
                throw std::runtime_error("Completed prerequisite has invalid Node::State");
            }
            bool stopBuild = (preqState != Node::State::Ok); // todo: && !_node->logBook()->keepWorking();
            if (stopBuild) {
                cancel();
            }
            if (_nExecutingPrerequisites == 0) handlePrerequisitesCompletion();
        }
    }

    void Node::handlePrerequisitesCompletion() {
        if (_canceling) {
            postCompletion(State::Canceled);
        }
        else if (!allNodesAreOk(_prerequisites)) {
            postCompletion(State::Failed);
        }
        else if (pendingStartSelf()) {
            _executionState = ExecutionState::Self;
            startSelf();
        }
        else {
            postCompletion(State::Ok);
        }
    }

    void Node::startSelf() {
        _context->statistics().registerSelfExecuted(this);
        auto d = Delegate<void>::CreateLambda([this]() { selfExecute(); });
        _context->threadPoolQueue().push(std::move(d));
    }


    void Node::postSelfCompletion(std::shared_ptr<SelfExecutionResult> result) {
        auto d = Delegate<void>::CreateLambda([this, result]() {
            handleSelfCompletion(result);
        });
        _context->mainThreadQueue().push(std::move(d));
    }

    void Node::handleSelfCompletion(std::shared_ptr<SelfExecutionResult> result) {

        if (result->_newState == Node::State::Ok) {
            _selfExecutionResult = result;
            startPreCommitNodes();
        } else {
            commitSelfCompletion(*result);
            notifyCompletion(result->_newState);
        }
    }

    void Node::startPreCommitNodes() {
        _executionState = ExecutionState::PreCommit;
        _preCommitNodes.clear();
        getPreCommitNodes(*_selfExecutionResult, _preCommitNodes);
        for (auto const& p : _preCommitNodes) startPreCommitNode(p.get());
        if (_nExecutingPreCommitNodes == 0) handlePreCommitNodesCompletion();
    }

    void Node::startPreCommitNode(Node* preCommitNode) {
        switch (preCommitNode->state()) {
        case Node::State::Dirty:
        {
            _nExecutingPreCommitNodes += 1;
#ifdef _DEBUG
            _executingPreCommitNodes.insert(preCommitNode);
#endif
            preCommitNode->start();
            break;
        }
        case Node::State::Executing:
        {
            _nExecutingPreCommitNodes += 1;
#ifdef _DEBUG
            // Multiply referenced node, already started by another
            // executor. Wait for it to complete.
            _executingPreCommitNodes.insert(preCommitNode);
#endif
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

    void Node::handlePreCommitNodeCompletion(Node* preCommitNode) {
        // When a preCommitNode completes it calls this function for all if
        // its postParents. There may be parents that are not part of the build 
        // scope. So ignore the callback when this node is in Idle state.
        //
        if (_executionState == ExecutionState::PreCommit) {
#ifdef _DEBUG
            if (0 == _executingPreCommitNodes.erase(preCommitNode)) {
                throw std::exception("Unknown pre-commit node");
            }
#endif
            if (0 == _nExecutingPreCommitNodes) {
                throw std::exception("Invalid _nExecutingPreCommitNodes");
            }
            _nExecutingPreCommitNodes -= 1;

            auto preqState = preCommitNode->state();
            bool preqCompleted =
                preqState == Node::State::Ok
                || preqState == Node::State::Failed
                || preqState == Node::State::Canceled
                || preqState == Node::State::Deleted;
            if (!preqCompleted) {
                throw std::runtime_error("Completed postrequisite has invalid Node::State");
            }
            bool stopBuild = (preqState != Node::State::Ok);
            if (stopBuild) {
                cancel();
            }
            if (_nExecutingPreCommitNodes == 0) handlePreCommitNodesCompletion();
        }
    }

    void Node::handlePreCommitNodesCompletion() {
        if (_canceling) {
            postCompletion(State::Canceled);
        } else if (!allNodesAreOk(_preCommitNodes)) {
            postCompletion(State::Failed);
        } else {
            commitSelfCompletion(*_selfExecutionResult);
            if (_selfExecutionResult->_newState != Node::State::Ok) {
                postCompletion(_selfExecutionResult->_newState);
            } else if (supportsPostrequisites()) {
                startPostrequisites();
            } else {
                postCompletion(State::Ok);
            }
        }
    }

    void Node::startPostrequisites() {
        _executionState = ExecutionState::Postrequisites;
        _postrequisites.clear();
        getPostrequisites(_postrequisites);
        for (auto const& p : _postrequisites) startPostrequisite(p.get());
        if (_nExecutingPostrequisites == 0) handlePostrequisitesCompletion();
    }

    void Node::startPostrequisite(Node* postrequisite) {
        // No subscription is made to the completed event of the prerequisites 
        // because the subscription overhead is too large.
        // Instead completion callback is done via handlePostrequisiteCompletion.
        switch (postrequisite->state()) {
        case Node::State::Dirty:
        {
            _nExecutingPostrequisites += 1;
#ifdef _DEBUG
            _executingPostrequisites.insert(postrequisite);
#endif
            postrequisite->start();
            break;
        }
        case Node::State::Executing:
        {
            _nExecutingPostrequisites += 1;
#ifdef _DEBUG
            // Multiply referenced node, already started by another
            // executor. Wait for it to complete.
            _executingPostrequisites.insert(postrequisite);
#endif
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

    void Node::handlePostrequisiteCompletion(Node* postrequisite) {
        // When a postrequisite completes it calls this function for all if
        // its postParents. There may be parents that are not part of the build 
        // scope. So ignore the callback when this node is in Idle state.
        //
        if (_executionState == ExecutionState::Postrequisites) {
#ifdef _DEBUG
            if (0 == _executingPostrequisites.erase(postrequisite)) {
                throw std::exception("Unknown postrequisite");
            }
#endif
            if (0 == _nExecutingPostrequisites) {
                throw std::exception("Invalid _nExecutingPostrequisites");
            }
            _nExecutingPostrequisites -= 1;

            auto preqState = postrequisite->state();
            bool preqCompleted =
                preqState == Node::State::Ok
                || preqState == Node::State::Failed
                || preqState == Node::State::Canceled
                || preqState == Node::State::Deleted;
            if (!preqCompleted) {
                throw std::runtime_error("Completed postrequisite has invalid Node::State");
            }
            bool stopBuild = (preqState != Node::State::Ok);
            if (stopBuild) {
                cancel();
            }
            if (_nExecutingPostrequisites == 0) handlePostrequisitesCompletion();
        }
    }

    void Node::handlePostrequisitesCompletion() {
        if (_canceling) {
            postCompletion(State::Canceled);
        } else if (!allNodesAreOk(_postrequisites)) {
            postCompletion(State::Failed);
        } else {
            postCompletion(State::Ok);
        }
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
        _preCommitNodes.clear();
        _postrequisites.clear();
        _nExecutingPrerequisites = 0;
        _nExecutingPreCommitNodes = 0;
        _nExecutingPostrequisites = 0;
        _selfExecutionResult = nullptr;
#ifdef _DEBUG
        _executingPrerequisites.clear();
        _executingPreCommitNodes.clear();
        _executingPostrequisites.clear();
#endif

        setState(newState);

        for (auto p : _dependants) {
            p->handlePrerequisiteCompletion(this);
            p->handlePreCommitNodeCompletion(this);
        }
        for (auto p : _postParents) p->handlePostrequisiteCompletion(this);
        _completor.Broadcast(this);
    }

    bool Node::busy() const {
        ASSERT_MAIN_THREAD(_context);
        return _state == State::Executing;
    }

    void Node::cancel() {
        if (!busy()) return;
        if (!_canceling) {
            _canceling = true;
            switch (_executionState) {
            case ExecutionState::Idle:
                break;
            case ExecutionState::Suspended:
                postCompletion(State::Canceled);
                break;
            case ExecutionState::Prerequisites:
                for (auto p : _prerequisites) {
                    p->cancel();
                }
                break;
            case ExecutionState::PreCommit:
                for (auto p : _preCommitNodes) {
                    p->cancel();
                }
                break;
            case ExecutionState::Postrequisites:
                for (auto p : _postrequisites) {
                    p->cancel();
                }
                break;
            case ExecutionState::Self:
                cancelSelf();
                break;
            default:
                throw std::runtime_error("invalid _executionState");
            }
        }
    }

    void Node::modified(bool newValue) {
        if (_modified != newValue) {
            _modified = newValue;
            if (_modified) {
                // TODO: register node as modified at context
            }
        }
    }
    bool Node::modified() const {
        return _modified;
    }

    void Node::stream(IStreamer* streamer) {
        streamer->stream(_name);
    }

    void Node::prepareDeserialize() {
    }

    void Node::restore(void* context) {
        _context = reinterpret_cast<ExecutionContext*>(context);
        _modified = false;
    }
}