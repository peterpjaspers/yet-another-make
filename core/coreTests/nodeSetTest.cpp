
#include "../NodeSet.h"
#include "../Node.h"
#include "../ExecutionContext.h"

#include "gtest/gtest.h"
#include <boost/process.hpp>

namespace
{
    using namespace YAM;

    class TestNode : public Node {
    public:
        TestNode(ExecutionContext* context, std::filesystem::path const& name)
            : Node(context, name)
        {}
        uint32_t typeId() const { return 0; }
    };

    class NodeSetUp
    {
    public:
        NodeSetUp() {
            n1 = std::make_shared<TestNode>(&context, "aap/noot");
            n1dup = std::make_shared<TestNode>(&context, "aap/noot");
            n2 = std::make_shared<TestNode>(&context, "aap/noot/mies");
        }

        ExecutionContext context;
        std::shared_ptr<Node> n1;
        std::shared_ptr<Node> n1dup;
        std::shared_ptr<Node> n2;
    };

    TEST(NodeSet, Add) {
        NodeSetUp setUp;
        NodeSet set;
        set.add(setUp.n1);
        EXPECT_EQ(1, set.size());
        EXPECT_ANY_THROW(set.add(setUp.n1dup));
    }

    TEST(NodeSet, AddIfAbsent) {
        NodeSetUp setUp;
        NodeSet set;
        set.add(setUp.n1);
        set.addIfAbsent(setUp.n1);
        EXPECT_EQ(1, set.size());
        set.addIfAbsent(setUp.n2);
        EXPECT_EQ(2, set.size());
    }

    TEST(NodeSet, Find) {
        NodeSetUp setUp;
        NodeSet set;
        set.add(setUp.n1);
        EXPECT_EQ(setUp.n1, set.find(setUp.n1->name()));
        EXPECT_EQ(nullptr, set.find(setUp.n2->name()));
    }

    TEST(NodeSet, Contains) {
        NodeSetUp setUp;
        NodeSet set;
        set.add(setUp.n1);
        EXPECT_TRUE(set.contains(setUp.n1->name()));
        EXPECT_FALSE(set.contains(setUp.n2->name()));
    }

    TEST(NodeSet, Remove) {
        NodeSetUp setUp;
        NodeSet set;
        set.add(setUp.n1);
        set.remove(setUp.n1);
        EXPECT_EQ(0, set.size());
        EXPECT_EQ(nullptr, set.find(setUp.n1->name()));
        EXPECT_FALSE(set.contains(setUp.n1->name()));
        EXPECT_ANY_THROW(set.remove(setUp.n1));
    }

    TEST(NodeSet, RemoveIfPresent) {
        NodeSetUp setUp;
        NodeSet set;
        set.add(setUp.n1);
        set.remove(setUp.n1);
        set.removeIfPresent(setUp.n1);
        EXPECT_EQ(0, set.size());
        EXPECT_EQ(nullptr, set.find(setUp.n1->name()));
    }
}