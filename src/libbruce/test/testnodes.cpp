#include <catch/catch.hpp>
#include <libbruce/bruce.h>

#include "testhelpers.h"
#include "nodes.h"

#include <stdio.h>

using namespace libbruce;

TEST_CASE("test findinternalkeyindex", "[nodes]")
{
    internalnode_ptr node = boost::make_shared<InternalNode>();
    node->insert(0, node_branch(one_r, nodeid_t(), 0));
    node->insert(1, node_branch(two_r, nodeid_t(), 0));
    node->insert(2, node_branch(three_r, nodeid_t(), 0));

    SECTION("find 1")
    {
        // The lowest index that could match this key
        REQUIRE( FindInternalKey(node, one_r, intToIntTree) == 0 );
    }
    SECTION("find 2")
    {
        // One to the left of the two, since some keys might have been inserted before the 2
        REQUIRE( FindInternalKey(node, two_r, intToIntTree) == 1 );
    }
    SECTION("find 5")
    {
        REQUIRE( FindInternalKey(node, intCopy(5), intToIntTree) == 2 );
    }
    SECTION("find 0")
    {
        // Completely to the left
        REQUIRE( FindInternalKey(node, intCopy(0), intToIntTree) == 0 );
    }
}
