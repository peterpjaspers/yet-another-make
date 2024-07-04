#include "../Delegates.h"
#include "../PriorityDispatcher.h"
#include "../DispatcherFrame.h"

#include "gtest/gtest.h"

namespace
{
    using namespace YAM;

    int x = 5;
    int y = 10;
    int sum = x + y;

    TEST(PriorityDispatcher, pushPopAndExecute) {
        Delegate<int, int> d;
        int r1 = -1;
        int r2 = -1;
        int r3 = -1;
        int r4 = -1;
        auto l1add = [&r1]() {r1 = 1; };
        auto l2add = [&r2]() {r2 = 2; };
        auto l3add = [&r3]() {r3 = 3; };
        auto l4add = [&r4]() {r4 = 4; };
        PriorityDispatcher q(3);
        q.push(Delegate<void>::CreateLambda(l1add), 0);
        q.push(Delegate<void>::CreateLambda(l2add), 1);
        q.push(Delegate<void>::CreateLambda(l3add), 2);
        q.push(Delegate<void>::CreateLambda(l4add), 2);

        Delegate<void> d3 = q.pop();
        d3.Execute();
        EXPECT_EQ(3, r3);

        Delegate<void> d4 = q.pop();
        d4.Execute();
        EXPECT_EQ(4, r4);

        Delegate<void> d2 = q.pop();
        d2.Execute();
        EXPECT_EQ(2, r2);

        Delegate<void> d1 = q.pop();
        d1.Execute();
        EXPECT_EQ(1, r1);
    }

    TEST(PriorityDispatcher, startStop) {
        Delegate<int, int> d;
        int r1 = -1;
        auto l1add = [&r1]() {r1 = x + y; };
        PriorityDispatcher q(4);

        q.stop();
        q.push(Delegate<void>::CreateLambda(l1add), 2);
        Delegate<void> d0 = q.pop();
        EXPECT_FALSE(d0.IsBound());

        q.start();
        Delegate<void> d1 = q.pop();
        EXPECT_TRUE(d1.IsBound());
        d1.Execute();
        EXPECT_EQ(sum, r1);
    }

    TEST(PriorityDispatcher, runFrame) {
        DispatcherFrame frame;
        Delegate<int, int> d;
        int r1 = -1;
        auto l1add = [&r1]() { r1 = x + y; };
        auto stop = [&frame]() { frame.stop(); };
        PriorityDispatcher q(4);

        q.push(Delegate<void>::CreateLambda(l1add), 1);
        q.push(Delegate<void>::CreateLambda(stop), 0);
        q.run(&frame);
        EXPECT_EQ(sum, r1);
    }
}