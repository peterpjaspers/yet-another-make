#include "../Delegates.h"
#include "../Dispatcher.h"
#include "../ThreadPool.h"

#include "gtest/gtest.h"
#include <atomic>

namespace
{
    using namespace YAM;

    const int x = 5;
    const int y = 10;
    const int sum = x + y;

    TEST(ThreadPool, processAndJoin) {
        std::atomic<int> r1 = -1;
        std::atomic<int> r2 = -1;
        std::atomic<int> count = 0;
        int nIterations = 1000;
        auto l1add = [&r1, &count]()
        {
            r1 = x + y;
            count++;
        };
        auto l2add = [&r2, &count]()
        {
            r2 = x + y;
            count++;
        };

        Dispatcher q;
        ThreadPool pool(&q, "YAM", 4);

        for (int i = 0; i < nIterations; ++i) {
            q.push(Delegate<void>::CreateLambda(l1add));
            q.push(Delegate<void>::CreateLambda(l2add));
        }
        pool.join();
        EXPECT_EQ(0, pool.size());
        EXPECT_EQ(2* nIterations, count);
        EXPECT_EQ(sum, r1);
        EXPECT_EQ(sum, r2);
    }

    TEST(ThreadPool, processAndChangeSize) {
        std::atomic<int> r1 = -1;
        std::atomic<int> r2 = -1;
        std::atomic<int> count = 0;
        int nIterations = 1000;
        auto l1add = [&r1, &count]()
        {
            r1 = x + y;
            count++;
        };
        auto l2add = [&r2, &count]()
        {
            r2 = x + y;
            count++;
        };

        Dispatcher q;
        ThreadPool pool(&q, "YAM", 4);
        
        q.suspend();
        for (int i = 0; i < nIterations; ++i) {
            q.push(Delegate<void>::CreateLambda(l1add));
            q.push(Delegate<void>::CreateLambda(l2add));
        }
        q.resume();
        pool.size(2); // reduce size while processing (is hopefully still) in progress
        EXPECT_EQ(2, pool.size());
        pool.size(6); // increase size while processing (is hopefully still) in progress
        EXPECT_EQ(6, pool.size());
        pool.join();
        EXPECT_EQ(0, pool.size());
        EXPECT_EQ(2 * nIterations, count);
        EXPECT_EQ(sum, r1);
        EXPECT_EQ(sum, r2);
    }
}