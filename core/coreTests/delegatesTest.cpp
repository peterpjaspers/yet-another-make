#include "gtest/gtest.h"

#include "../Delegates.h"

namespace
{
	int x = 5;
	int y = 10;
	int sum = x + y;

	int add(int x, int y) { return x + y; }

	class Adder
	{
	private:
		int _x;
	public:
		Adder(int x) : _x(x) {}
		int add(int y) { return _x + y; }
	};

	TEST(Delegate, Lambda) {
		Delegate<int, int, int> d;
		auto ladd = [](int x, int y)
		{
			return x + y;
		};
		d.BindLambda(ladd);
		EXPECT_EQ(sum, d.Execute(x, y));

		d = Delegate<int, int, int>::CreateLambda(ladd);
		EXPECT_EQ(sum, d.Execute(x, y));

		Delegate<int, int, int> clone = d;
		EXPECT_EQ(sum, clone.Execute(x, y));
	}

	TEST(Delegate, Raw) {
		Adder adder(x);
		Delegate<int, int> d;

		d.BindRaw(&adder, &Adder::add);
		EXPECT_EQ(sum, d.Execute(y));

		d = Delegate<int, int>::CreateRaw(&adder, &Adder::add);
		EXPECT_EQ(sum, d.Execute(y));

		Delegate<int, int> clone = d;
		EXPECT_EQ(sum, clone.Execute(y));
	}

	TEST(Delegate, Static) {
		Delegate<int, int, int> d;

		d.BindStatic(&add);
		EXPECT_EQ(sum, d.Execute(x, y));

		d = Delegate<int, int, int>::CreateStatic(&add);
		EXPECT_EQ(sum, d.Execute(x, y));

		Delegate<int, int, int> clone = d;
		EXPECT_EQ(sum, clone.Execute(x, y));
	}

	TEST(Delegate, MultiCast) {
		MulticastDelegate<int, int> d;
		int r1 = -1;
		int r2 = -1;
		auto l1add = [&r1](int x, int y)
		{
			r1 = x + y;
		};
		auto l2add = [&r2](int x, int y)
		{
			r2 = x + y;
		};
		auto h1 = d += l1add;
		auto h2 = d += l2add;
		d.Broadcast(x, y);
		EXPECT_EQ(sum, r1);
		EXPECT_EQ(sum, r2);

		r1 = -1;
		r2 = -1;
		d -= h1;
		d.Broadcast(x, y);
		EXPECT_EQ(-1, r1);
		EXPECT_EQ(sum, r2);

		r1 = -1;
		r2 = -1;
		d -= h2;
		d.Broadcast(x, y);
		EXPECT_EQ(-1, r1);
		EXPECT_EQ(-1, r2);
	}
}