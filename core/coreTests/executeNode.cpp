#include "gtest/gtest.h"
#include "executeNode.h"
#include "../CommandNode.h"
#include "../ExecutionContext.h"
#include "../Dispatcher.h"
#include "../DispatcherFrame.h"
#include <map>
#include <thread>
#include <mutex>

namespace YAMTest
{
    using namespace YAM;

    std::chrono::seconds timeout(1000);

    bool executeNode(Node* n) {
		std::vector<YAM::Node*> nodes{ n };
		return executeNodes(nodes);
    }

	bool executeNodes(std::vector<std::shared_ptr<Node>> const& nodes) {
		std::vector<YAM::Node*> nodePtrs;
		for (auto const& n : nodes) nodePtrs.push_back(n.get());
		return executeNodes(nodePtrs);
	}

	class Executor
	{
	public:
		Executor(std::vector<Node*> const& nodes)
			: _nodes(nodes)
			, _nCompleted(0)
		{}

		void execute() {
			for (auto n : _nodes) {
				_handles[n] = n->completor().Add(
					Delegate<void, Node*>::CreateRaw(this, &Executor::_handleNodeCompletion));
				n->start();
			}
			_nodes[0]->context()->mainThreadQueue().run(&_frame);

			for (auto& pair : _handles) {
				pair.first->completor().Remove(pair.second);
			}
			for (auto node : _nodes) {
				bool completed =
					node->state() == Node::State::Ok
					|| node->state() == Node::State::Failed
					|| node->state() == Node::State::Canceled;
				if (completed) _nCompleted++;
			}
			_dispatcher.stop();
		}

		bool wait() {
			_dispatcher.run();
			return _nCompleted == _nodes.size();
		}

	private:
		std::vector<Node*> _nodes;
		std::vector<Node*> _completed;
		std::map<Node*, DelegateHandle> _handles;
		Dispatcher _dispatcher;
		DispatcherFrame _frame;
		std::atomic<unsigned int> _nCompleted;

		void _handleNodeCompletion(Node* node) {
			bool complete =
				node->state() == Node::State::Ok
				|| node->state() == Node::State::Failed
				|| node->state() == Node::State::Canceled;
			if (!complete) {
				ASSERT_TRUE(complete);
			}
			_completed.push_back(node);
			if (_completed.size() == _nodes.size()) {
				_nodes[0]->context()->mainThreadQueue().push(Delegate<void>::CreateLambda(
					[this]() { _frame.stop(); }));
			}
		}

	};

	bool executeNodes(std::vector<Node*> const& nodes) {
		if (nodes.empty()) return true;
		Executor ex(nodes);
		ExecutionContext* context = nodes[0]->context();
		context->mainThreadQueue().push(Delegate<void>::CreateLambda(
			[&ex]() {ex.execute(); }));
		return ex.wait();
	}
}
