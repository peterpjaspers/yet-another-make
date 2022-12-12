#include "gtest/gtest.h"
#include "executeNode.h"
#include "../ExecutionContext.h"
#include <map>
#include <thread>
#include <mutex>

namespace YAMTest
{
    using namespace YAM;

    std::chrono::seconds timeout(10);

    bool executeNode(Node* n) {
		std::vector<YAM::Node*> nodes{ n };
		return executeNodes(nodes);
    }

    bool executeNodes(std::vector<YAM::Node*> const & nodesRef) {
		std::vector<YAM::Node*> nodes(nodesRef);
		std::map<Node*, bool> completed;
		std::map<Node*, DelegateHandle> handles;
		std::mutex mtx;
		std::condition_variable cv;

		// make sure to start all nodes in YAM mainthread. If not, race-conditions
		// will occur between start() code executed in testthread and prerequisite 
		// completion handling in mainthread.
		auto& mainThreadQueue = nodes[0]->context()->mainThreadQueue();
		bool started = false;
		auto startAll = Delegate<void>::CreateLambda([&]()
			{
				for (std::size_t i = 0; i < nodes.size(); ++i) {
					Node* n = nodes[i];
					completed[n] = false;
					handles[n] = n->completor() += [&, n](Node* node)
					{
						ASSERT_EQ(n, node);
						{
							std::lock_guard lk(mtx);
							ASSERT_FALSE(completed[node]);
							completed[node] = true;
						}
						cv.notify_one();
					};
				}
				for (std::size_t i = 0; i < nodes.size(); ++i) nodes[i]->start();
				{
					std::lock_guard lk(mtx);
					started = true;
				}
				cv.notify_one();
			});
		mainThreadQueue.push(std::move(startAll));

		std::unique_lock ulk(mtx);
		cv.wait_for(ulk, std::chrono::seconds(timeout), [&]()
			{
				return started;
			});

		bool allCompleted = started && cv.wait_for(ulk, std::chrono::seconds(timeout), [&]()
			{
				bool ac = true;
				for (std::size_t i = 0; i < nodes.size(); ++i) ac &= completed[nodes[i]];
				return ac;
			});

		bool unsubscribed = false;
		auto unsubscribe = Delegate<void>::CreateLambda([&]()
			{
				for (std::size_t i = 0; i < nodes.size(); ++i) {
					Node* n = nodes[i];
					n->completor() -= handles[n];
				}
				{
					std::lock_guard lk(mtx);
					unsubscribed = true;
				}
				cv.notify_one();
			});
		mainThreadQueue.push(std::move(unsubscribe));

		cv.wait_for(ulk, std::chrono::seconds(timeout), [&]()
			{
				return unsubscribed;
			});

		return allCompleted;
    }
}
