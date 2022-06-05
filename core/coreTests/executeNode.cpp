#include "gtest/gtest.h"
#include "executeNode.h"
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
				EXPECT_EQ(n, node);
				{
					std::lock_guard lk(mtx);
					EXPECT_FALSE(completed[node]);
					completed[node] = true;
				}
				cv.notify_one();
			};
		}

		for (std::size_t i = 0; i < nodes.size(); ++i) nodes[i]->start();

		bool allCompleted = false;
		std::unique_lock ulk(mtx);
		cv.wait_for(ulk, std::chrono::seconds(timeout), [&]()
			{
				allCompleted = true;
				for (std::size_t i = 0; i < nodes.size(); ++i) allCompleted &= completed[nodes[i]];
				return allCompleted;
			});

		for (std::size_t i = 0; i < nodes.size(); ++i) {
			Node* n = nodes[i];
			n->completor() -= handles[n];
		}

		return allCompleted;
    }
}
