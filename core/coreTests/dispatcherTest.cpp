#include "../Delegates.h"
#include "../Dispatcher.h"

#include "gtest/gtest.h"

namespace
{
	using namespace YAM;

	int x = 5;
	int y = 10;
	int sum = x + y;

	TEST(Dispatcher, pushPopAndExecute) {
		Delegate<int, int> d;
		int r1 = -1;
		int r2 = -1;
		auto l1add = [&r1]()
		{
			r1 = x + y;
		};
		auto l2add = [&r2]()
		{
			r2 = x + y;
		};
		Dispatcher q;
		q.push(Delegate<void>::CreateLambda(l1add));
		q.push(Delegate<void>::CreateLambda(l2add));

		Delegate<void> d1 = q.pop();
		Delegate<void> d2 = q.pop();

		d1.Execute();
		d2.Execute();

		EXPECT_EQ(sum, r1);
		EXPECT_EQ(sum, r2);
	}

	TEST(Dispatcher, startStop) {
		Delegate<int, int> d;
		int r1 = -1;
		auto l1add = [&r1]()
		{
			r1 = x + y;
		};
		Dispatcher q;

		q.stop();
		q.push(Delegate<void>::CreateLambda(l1add));
		Delegate<void> d0 = q.pop();
		EXPECT_FALSE(d0.IsBound());

		q.start();
		Delegate<void> d1 = q.pop();
		EXPECT_TRUE(d1.IsBound());
		d1.Execute();
		EXPECT_EQ(sum, r1);
	}
}