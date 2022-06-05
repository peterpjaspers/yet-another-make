
#include "../NodeSet.h"
#include "NumberNode.h"
#include "../ExecutionContext.h"

#include "gtest/gtest.h"
#include <boost/process.hpp>

namespace
{
	using namespace YAM;
	using namespace YAMTest;

	ExecutionContext context;
	NumberNode n1(&context, "aap/noot");
	NumberNode n1dup(&context, "aap/noot");
	NumberNode n2(&context, "aap/noot/mies");

	TEST(NodeSet, Add) {
		NodeSet set;
		set.add(&n1);
		EXPECT_EQ(1, set.size());
		EXPECT_ANY_THROW(set.add(&n1dup));
	}

	TEST(NodeSet, AddIfAbsent) {
		NodeSet set;
		set.add(&n1);
		set.addIfAbsent(&n1);
		EXPECT_EQ(1, set.size());
		set.addIfAbsent(&n2);
		EXPECT_EQ(2, set.size());
	}

	TEST(NodeSet, Find) {
		NodeSet set;
		set.add(&n1);
		EXPECT_EQ(&n1, set.find(n1.name()));
		EXPECT_EQ(nullptr, set.find(n2.name()));
	}

	TEST(NodeSet, Contains) {
		NodeSet set;
		set.add(&n1);
		EXPECT_TRUE(set.contains(n1.name()));
		EXPECT_FALSE(set.contains(n2.name()));
	}

	TEST(NodeSet, Remove) {
		NodeSet set;
		set.add(&n1);
		set.remove(&n1);
		EXPECT_EQ(0, set.size());
		EXPECT_EQ(nullptr, set.find(n1.name()));
		EXPECT_FALSE(set.contains(n1.name()));
		EXPECT_ANY_THROW(set.remove(&n1));
	}

	TEST(NodeSet, RemoveIfPresent) {
		NodeSet set;
		set.add(&n1);
		set.remove(&n1);
		set.removeIfPresent(&n1);
		EXPECT_EQ(0, set.size());
		EXPECT_EQ(nullptr, set.find(n1.name()));
	}
}