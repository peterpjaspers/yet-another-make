#include "executeNode.h"
#include "AdditionNode.h"
#include "NumberNode.h"
#include "../ExecutionContext.h"
#include "../../xxhash/xxhash.h"

#include "gtest/gtest.h"
#include <stdlib.h>
#include <thread>
#include <chrono>

namespace
{
	std::chrono::seconds timeout(10);

	using namespace YAMTest;
	TEST(NumberNode, executeOne) {
		ExecutionContext context;
		NumberNode nr(&context, "number");
		bool completed = executeNode(&nr);
		EXPECT_TRUE(completed);
		EXPECT_EQ(Node::State::Ok, nr.state());
	}

	TEST(NumberNode, executeMany) {
		ExecutionContext context;
		const std::size_t n = 4;
		std::vector<Node*> nrs;
		for (std::size_t i = 0; i < n; ++i) {
			nrs.push_back(new NumberNode(&context, "number"));
		}

		bool completed = executeNodes(nrs);
		EXPECT_TRUE(completed);

		for (std::size_t i = 0; i < n; ++i) delete nrs[i];
	}

	TEST(AdditionNode, executeMany) {
		std::size_t const n = 4;
		std::vector<NumberNode*> ops;
		int sum0 = 0;
		int sum1 = 0;

		ExecutionContext context;
		AdditionNode addition0(&context, "addition0");
		for (std::size_t i = 0; i < n; ++i) {
			NumberNode* nr = new NumberNode(&context, "number");
			nr->number((int)i);
			ops.push_back(nr);
			addition0.addOperand(ops[i]);
			sum0 += (int)i;
		}
		AdditionNode addition1(&context, "addition1");
		for (std::size_t i = 1; i < n; ++i) {
			addition1.addOperand(ops[i]);
			sum1 += (int)i;
		}

		std::vector<Node*> additions;
		additions.push_back(&addition0);
		additions.push_back(&addition1);
		bool completed = executeNodes(additions);

		ASSERT_TRUE(completed);
		EXPECT_EQ(Node::State::Ok, addition0.state());
		EXPECT_EQ(Node::State::Ok, addition1.state());
		EXPECT_EQ(sum0, addition0.sum()->number());
		EXPECT_EQ(sum1, addition1.sum()->number());
		EXPECT_EQ(addition0.executionHash(), addition0.computeExecutionHash());
		EXPECT_EQ(addition1.executionHash(), addition1.computeExecutionHash());

		for (std::size_t i = 0; i < n; ++i) {
			EXPECT_EQ(Node::State::Ok, ops[i]->state());
			delete ops[i];
		}
	}

	TEST(AdditionNode, reexecute) {
		std::size_t const nOperands = 4;
		std::vector<NumberNode*> ops;
		std::mutex mtx;
		std::condition_variable cv;

		bool initialized = false;
		ExecutionContext context;
		AdditionNode addition(&context, "addition");
		int sum = 0;
		auto d0 = Delegate<void>::CreateLambda([&]()
			{
				std::lock_guard lk(mtx);
				for (std::size_t i = 0; i < nOperands; ++i) {
					ops.push_back(new NumberNode(&context, "number"));
					ops[i]->number((int)i);
					addition.addOperand(ops[i]);
					sum += (int)i;
				}
				initialized = true;
				cv.notify_one();
			});
		context.mainThreadQueue().push(std::move(d0));
		std::unique_lock ulk(mtx);
		cv.wait_for(ulk, std::chrono::seconds(timeout), [&]() { return initialized;});

		bool completed = executeNode(&addition);
		ASSERT_TRUE(completed);

		bool tampered = false;
		auto d1 = Delegate<void>::CreateLambda([&]()
			{
				std::lock_guard lk(mtx);
				EXPECT_EQ(Node::State::Ok, addition.state());
				EXPECT_EQ(sum, addition.sum()->number());
				EXPECT_EQ(addition.executionHash(), addition.computeExecutionHash());
				for (std::size_t i = 0; i < ops.size(); ++i) {
					EXPECT_EQ(Node::State::Ok, ops[i]->state());
				}
				// tamper with the addition result
				addition.sum()->number(9999);
				EXPECT_EQ(Node::State::Dirty, addition.state());
				tampered = true;
				cv.notify_one();

			});
		context.mainThreadQueue().push(std::move(d1));
		cv.wait_for(ulk, std::chrono::seconds(timeout), [&]() {return tampered; });

		completed = executeNode(&addition);
		ASSERT_TRUE(completed);

		int delta = 256;
		bool opModified = false;
		auto d3 = Delegate<void>::CreateLambda([&]()
			{
				std::lock_guard lk(mtx);
				EXPECT_EQ(Node::State::Ok, addition.state());
				EXPECT_EQ(sum, addition.sum()->number());
				EXPECT_EQ(addition.executionHash(), addition.computeExecutionHash());
				for (std::size_t i = 0; i < ops.size(); ++i) {
					EXPECT_EQ(Node::State::Ok, ops[i]->state());
				}
				// modify one of the operands
				ops[0]->number(ops[0]->number() + delta);
				EXPECT_EQ(Node::State::Dirty, addition.state());
				opModified = true;
				cv.notify_one();
			});
		context.mainThreadQueue().push(std::move(d3));
		cv.wait_for(ulk, std::chrono::seconds(timeout), [&]() {return opModified; });

		completed = executeNode(&addition);
		ASSERT_TRUE(completed);

		bool verified = false;
		auto d4 = Delegate<void>::CreateLambda([&]()
			{
				std::lock_guard lk(mtx);
				EXPECT_EQ(Node::State::Ok, addition.state());
				EXPECT_EQ(sum + delta, addition.sum()->number());
				EXPECT_EQ(addition.executionHash(), addition.computeExecutionHash());
				for (std::size_t i = 0; i < ops.size(); ++i) {
					EXPECT_EQ(Node::State::Ok, ops[i]->state());
				}
				verified = true;
				cv.notify_one();
			});
		context.mainThreadQueue().push(std::move(d4));
		cv.wait_for(ulk, std::chrono::seconds(timeout), [&]() {return verified; });

		context.threadPool().join();
		context.mainThreadQueue().push(Delegate<void>::CreateRaw(&(context.mainThreadQueue()), &Dispatcher::stop));
		context.mainThread().join();

		addition.clearOperands(true);
	}
}