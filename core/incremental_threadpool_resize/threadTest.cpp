#include "pch.h"

#include "../Delegates.h"
#include "../Dispatcher.h"
#include "../Thread.h"

#include "gtest/gtest.h"

namespace
{
	using namespace YAM;

	int x = 5;
	int y = 10;
	int sum = x + y;

	TEST(Thread, processAndStop) {
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
		Thread t1(&q, std::string("t1"));
		Thread t2(&q, std::string("t2"));

		q.push(Delegate<void>::CreateLambda(l1add));
		q.push(Delegate<void>::CreateLambda(l2add));
		q.push(Delegate<void>::CreateLambda([&t1, &t2, &q]() { t1.stop(); t2.stop(); q.stop(); }));

		t1.join();
		t2.join();

		EXPECT_EQ(sum, r1);
		EXPECT_EQ(sum, r2);
	}
}