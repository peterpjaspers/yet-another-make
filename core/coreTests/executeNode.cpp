#include "gtest/gtest.h"
#include "executeNode.h"
#include "../ExecutionContext.h"
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

    bool executeNodes(std::vector<YAM::Node*> const & nodesRef) {
		std::vector<YAM::Node*> nodes(nodesRef);
		std::map<Node*, bool> completed;
		std::map<Node*, DelegateHandle> handles;
		std::mutex mtx;
		std::condition_variable cv;

		if (nodes.empty()) return true;
		// make sure to start all nodes in YAM mainthread. If not, race-conditions
		// will occur between start() code executed in this (i.e. the test) thread and 
		// prerequisite completion handling in yam mainthread.
		auto& mainThreadQueue = nodes[0]->context()->mainThreadQueue();
		bool started = false;
		auto start = Delegate<void>::CreateLambda([&]()
			{
				std::lock_guard lk(mtx);
				for (std::size_t i = 0; i < nodes.size(); ++i) {
					Node* n = nodes[i];
					completed[n] = false;
					handles[n] = n->completor() += [&, n](Node* node)
					{
						std::lock_guard lk(mtx);
						ASSERT_EQ(n, node);
						ASSERT_FALSE(completed[node]);
						completed[node] = true;
						cv.notify_one();
					};
				}
				for (std::size_t i = 0; i < nodes.size(); ++i) nodes[i]->start();
				started = true;
				cv.notify_one();
			});
		mainThreadQueue.push(std::move(start));
		std::unique_lock ulk(mtx);
		bool allCompleted = cv.wait_for(ulk, std::chrono::seconds(timeout), [&]()
			{
				bool ac = started;
				for (std::size_t i = 0; ac && (i < nodes.size()); ++i) ac &= completed[nodes[i]];
				return ac;
			});

		// unsubscribe must happen in mainthread to avoid race-condition: unsubscribe in this
		// thread while node completor() broadcast is still in progress in mainthread. This
		// broadcast will callback the lambda in lines 38-44. The notify_one() at line 43 will
		// finish the wait for allCompleted at line 52, after which the unsubscribe will be done.
		// This race will cause the MulticastDelegate to not remove the callback: see Delegate.h, 
		// MulticastDelegate::Remove, line 997. The event cannot be removed from m_events because 
		// the broadcast is still iterating over m_events. Instead the callback is cleared, leaving
		// event to be unbound event. This will cause assert at Delegates.h line 812 to fail at next
		// executeNode call.
		// For single-threaded access this problem can be solved in MulticastDelegate::Broadcast. 
		// This function checks whether event handle is valid. The Remove function however only clears 
		// the callback. The Broadcast function must therefore check whether the callback is bound.
		// Probably the simplest solution is to forbid Remove while the multicast delegate is locked. 
		// But both solutions do not protect the delegate for multi-threaded access.
		bool unsubscribed = false;
		auto unsubscribe = Delegate<void>::CreateLambda([&]()
			{
				std::lock_guard lk(mtx);
				for (std::size_t i = 0; i < nodes.size(); ++i) {
					Node* n = nodes[i];
					n->completor() -= handles[n];
				}
				unsubscribed = true;
				cv.notify_one();
			});
		mainThreadQueue.push(std::move(unsubscribe));
		cv.wait_for(ulk, std::chrono::seconds(timeout), [&]() { return unsubscribed; });

		return allCompleted;
    }
}
