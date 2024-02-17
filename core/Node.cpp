#include "Node.h"
#include "NodeSet.h"
#include "Delegates.h"
#include "ExecutionContext.h"
#include "FileRepository.h"
#include "IStreamer.h"

#include <iostream>

namespace {
    using namespace YAM;

    bool allNodesAreOk(std::unordered_set<Node*> const& nodes) {
        for (auto const& n : nodes) {
            if (n->state() == Node::State::Deleted) continue;
            if (n->state() != Node::State::Ok) return false;
        }
        return true;
    }

    bool isFailedOrCanceled(Node const* node) {
        return (
            node->state() == Node::State::Failed
            || node->state() == Node::State::Canceled);
    }
}

namespace YAM
{
    Node::Node()
        : _context(nullptr)
        , _state(Node::State::Dirty)
        , _canceling(false)
        , _nExecutingNodes(0)
        , _notifyingObservers(false)
        , _modified(false) // because this instance will be deserialized
    {}

    Node::Node(ExecutionContext* context, std::filesystem::path const& name)
        : _context(context)
        , _name(name)
        , _state(Node::State::Dirty)
        , _canceling(false)
        , _nExecutingNodes(0)
        , _notifyingObservers(false)
        , _modified(true)
    {}

    Node::~Node() {}

    std::shared_ptr<FileRepository> const& Node::repository() const {
        auto repoName = FileRepository::repoNameFromPath(name());
        return context()->findRepository(repoName);
    }

    std::filesystem::path Node::absolutePath() const {
        return repository()->absolutePathOf(name());
    }

    void Node::setState(State newState) {
        if (_state != newState) {
            if (_state == Node::State::Deleted) {
                throw std::runtime_error("Not allowwed to update state of Deleted object, user undelete");
            }
            bool wasExecuting = _state == Node::State::Executing;
            _state = newState;
            bool nowCompleted =
                _state == Node::State::Ok
                || _state == Node::State::Failed
                || _state == Node::State::Canceled;
            _notifyingObservers = true;
            if (_state == Node::State::Dirty) {
                for (auto observer : _observers) {
                    observer->handleDirtyOf(this);
                }
            }
            if (wasExecuting && nowCompleted) {
                for (auto observer : _observers) {
                    observer->handleCompletionOf(this);
                }
            }
            _notifyingObservers = false;
            for (auto const& pair : _addedAndRemovedObservers) {
                if (pair.second) {
                    addObserver(pair.first);
                } else {
                    removeObserver(pair.first);
                }
            }
            _addedAndRemovedObservers.clear();
        }
    }

    void Node::undelete() {
        if (_state != Node::State::Deleted) {
            throw std::runtime_error("Not allowwed to undelete an object that is not in deleted state");
        }
        _state = Node::State::Dirty;
    }

    void Node::addObserver(StateObserver* observer) {
        if (_notifyingObservers) {
            _addedAndRemovedObservers.push_back({ observer, true });
            return;
        }
        auto p = _observers.insert(observer);
        if (!p.second) {
            throw std::runtime_error("Attempt to add duplicate state observer");
        }
    }

    void Node::removeObserver(StateObserver* observer) {
        if (_notifyingObservers) {
            _addedAndRemovedObservers.push_back({ observer, false });
            return;
        }
        if (0 == _observers.erase(observer)) throw std::runtime_error("Attempt to remove unknown state observer");
    }

    void Node::start() {
        ASSERT_MAIN_THREAD(_context);
        if (state() != Node::State::Dirty) throw std::runtime_error("Attempt to start while not dirty");
        _context->statistics().registerStarted(this);
        setState(Node::State::Executing);
    }

    void Node::startNodes(
            std::vector<Node*> const& nodes,
            Delegate<void, Node::State> const& callback
    ) {
        ASSERT_MAIN_THREAD(_context);
        if (_state != Node::State::Executing) throw std::runtime_error("Attempt to start nodes while not in executing state");
        if (_nExecutingNodes != 0) throw std::runtime_error("Attempt to start nodes while already executing nodes");
        bool stop = false;
        for (auto n : nodes) {
            stop = stop || isFailedOrCanceled(n);
            _nodesToExecute.insert(n);
        }
        _callback = callback;
        if (stop) {
            cancel();
        } else {
            for (auto n : _nodesToExecute) startNode(n);
        }
        if (_nExecutingNodes == 0) _handleNodesCompletion();
    }

    void Node::startNode(Node* node) {
#ifdef _DEBUG
        if (!node->observers().contains(this)) {
            throw std::runtime_error("Started node is not observed");
        }
#endif
        switch (node->state()) {
        case Node::State::Dirty:
        {
            _nExecutingNodes += 1;
#ifdef _DEBUG
            _executingNodes.insert(node);
#endif
            node->start();
            break;
        }
        case Node::State::Executing:
        {
            _nExecutingNodes += 1;
#ifdef _DEBUG
            _executingNodes.insert(node);
#endif
            break;
        }
        case Node::State::Ok: break;
        case Node::State::Failed: break;
        case Node::State::Canceled: break;
        case Node::State::Deleted:break;
        default:
        throw std::runtime_error("Unknown Node::State");
        }
    }

    void Node::handleDirtyOf(Node* observedNode) { 
        if (_state != Node::State::Deleted) {
            setState(Node::State::Dirty);
        }
    }

    // This node subscribed at 'node' to be notified of completion of its
    // execution.
    void Node::handleCompletionOf(Node* node) {
        // When a node completes it calls this function for ALL if its 
        // observers. This node must only handle completion of node when
        // it is actually waiting for node to complete.
        if (_nodesToExecute.find(node) == _nodesToExecute.end()) return;
#ifdef _DEBUG
        if (_nExecutingNodes != _executingNodes.size()) {
            throw std::exception("_nExecutingNodes != _executingNodes.size()");
        }
        if (0 == _executingNodes.erase(node)) {
            throw std::exception("callback from unexpected node");
        }
#endif
        if (0 == _nExecutingNodes) {
            throw std::exception("_nExecutingNodes cannot drop below zero");
        }
        _nExecutingNodes -= 1;

        auto nodeState = node->state();
        bool nodeCompleted =
            nodeState == Node::State::Ok
            || nodeState == Node::State::Failed
            || nodeState == Node::State::Canceled;
        if (!nodeCompleted) {
            throw std::runtime_error("Executing node notifies unexpected state change");
        }
        bool stopBuild = (nodeState != Node::State::Ok); // todo: && !_node->logBook()->keepWorking();
        if (stopBuild) {
            cancel();
        }
        if (_nExecutingNodes == 0) _handleNodesCompletion();
    }

    void Node::_handleNodesCompletion() {
        bool allOk = allNodesAreOk(_nodesToExecute);
        _nodesToExecute.clear();
#ifdef _DEBUG
        _executingNodes.clear();
#endif
        Node::State state;
        if (_canceling) {
            state = State::Canceled;
        } else if (allOk) {
            state = State::Ok;
        } else {
            state = State::Failed;
        }
        auto d = Delegate<void>::CreateLambda([this, state]()
        {
            _callback.Execute(state);
        });
        _context->mainThreadQueue().push(std::move(d));
    }

    void Node::postCompletion(Node::State newState) {
        auto d = Delegate<void>::CreateLambda([this, newState]()
        {
            notifyCompletion(newState);
        });
        _context->mainThreadQueue().push(std::move(d));
    }

    void Node::notifyCompletion(Node::State newState) {
        ASSERT_MAIN_THREAD(_context);
        if (_state != Node::State::Executing) throw std::exception("cannot complete when not executing");
        if (0 < _nExecutingNodes) throw std::exception("cannot complete when executing nodes");
        if (!_nodesToExecute.empty()) throw std::exception("_nodesToExecute must be empty");
#ifdef _DEBUG
        if (_nExecutingNodes != _executingNodes.size()) throw std::exception("_nExecutingNodes != _executingNodes.size()");
#endif
        _canceling = false;
        //std::stringstream ss;
        //ss << _name.string() << " completed with state " << static_cast<int>(newState);
        //LogRecord progress(LogRecord::Progress, ss.str());
        //_context->logBook()->add(progress);
        setState(newState);
        _completor.Broadcast(this);
    }

    void Node::cancel() {
        if (_state != Node::State::Executing) return;
        if (!_canceling) {
            _canceling = true;
            for (auto p : _nodesToExecute) {
                p->cancel();
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
        uint32_t state;
        if (streamer->writing()) state = static_cast<uint32_t>(_state);
        streamer->stream(state);
        if (streamer->reading()) _state = static_cast<Node::State>(state);
    }

    void Node::prepareDeserialize() {
    }

    bool Node::restore(void* context, std::unordered_set<IPersistable const*>& restored) {
        bool inserted = restored.insert(this).second;
        if (inserted) {
            _context = reinterpret_cast<ExecutionContext*>(context);
            _modified = false;
        }
        return inserted;
    }
}