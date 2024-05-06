#include "gtest/gtest.h"

#include "../Delegates.h"

#include <chrono>
#include <iostream>

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

    // On dual core i5-4210M CPU @ 2.60 GHz
    // 16 GB RAM, Windows 10, release build:
    //    subscribe ns = 108
    //    broadcast ns = 19
    //    unsubscribe ns = 40
    //    total ns = 169
    //    callback ns = 0
    TEST(Delegate, MultiCastOverhead) {
        const std::size_t count = 100000;

        std::vector<MulticastDelegate<int, int>> delegates;
        for (std::size_t i = 0; i < count; ++i) {
            delegates.push_back(MulticastDelegate<int, int>());
        }
        int r1 = 0;
        auto callback = [&r1](int x, int y) {
            r1 += x + y;
        };

        auto start = std::chrono::high_resolution_clock::now();
        std::vector<DelegateHandle> handles;
        for (std::size_t i = 0; i < count; ++i) {
            auto& d = delegates[i];
            auto h = d += callback;
            handles.push_back(h);
        }
        auto subscribe = std::chrono::high_resolution_clock::now();

        for (std::size_t i = 0; i < count; ++i) {
            delegates[i].Broadcast(1, 1);
        }
        auto broadcast = std::chrono::high_resolution_clock::now();

        EXPECT_EQ(2*count, r1);
        for (std::size_t i = 0; i < count; ++i) {
            delegates[i] -= handles[i];
        }
        auto unsubscribe = std::chrono::high_resolution_clock::now();

        auto subscribeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(subscribe - start);
        auto broadcastNs = std::chrono::duration_cast<std::chrono::nanoseconds>( broadcast - subscribe);
        auto unsubscribeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(unsubscribe - broadcast);
        auto totalNs = std::chrono::duration_cast<std::chrono::nanoseconds>(unsubscribe - start);

        std::cout
            << "subscribe ns=" << subscribeNs.count()/count
            << std::endl
            << "broadcast ns=" << broadcastNs.count() / count
            << std::endl
            << "unsubscribe ns=" << unsubscribeNs.count() / count
            << std::endl
            << "total ns=" << totalNs.count()/count
            << std::endl;


        auto startcb = std::chrono::high_resolution_clock::now();
        for (std::size_t i = 0; i < count; ++i) {
            callback(1, 1);
        }
        auto endcb = std::chrono::high_resolution_clock::now();
        auto cbNs = std::chrono::duration_cast<std::chrono::nanoseconds>(endcb - startcb);
        std::cout
            << "callback ns=" << cbNs.count()/count
            << std::endl;
    }
}