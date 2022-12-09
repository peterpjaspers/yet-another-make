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

    bool executeNodes(std::vector<YAM::Node*> const & nodes) {
		std::map<Node*, bool> completed;
		std::map<Node*, DelegateHandle> handles;
		std::mutex mtx;
		std::condition_variable cv;

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

		// make sure to start all nodes in YAM mainthread. If not, race-conditions
		// will occur between start() code executed in testthread and prerequisite 
		// completion handling in mainthread.
		auto& mainThreadQueue = nodes[0]->context()->mainThreadQueue();
		auto startAll = Delegate<void>::CreateLambda([&nodes]()
			{
				for (std::size_t i = 0; i < nodes.size(); ++i) nodes[i]->start();
			});
		mainThreadQueue.push(std::move(startAll));

		std::unique_lock ulk(mtx);
		bool allCompleted = cv.wait_for(ulk, std::chrono::seconds(timeout), [&]()
			{
				bool ac = true;
				for (std::size_t i = 0; i < nodes.size(); ++i) ac &= completed[nodes[i]];
				return ac;
			});

		if (!allCompleted) {
			throw std::runtime_error("not all nodes completed their execution");
		}

		for (std::size_t i = 0; i < nodes.size(); ++i) {
			Node* n = nodes[i];
			n->completor() -= handles[n];
		}

		return allCompleted;
    }
}
