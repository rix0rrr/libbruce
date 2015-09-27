#include <catch/catch.hpp>
#include <libbruce/bruce.h>
#include "serializing.h"
#include "testhelpers.h"

#include <stdio.h>

using namespace libbruce;


TEST_CASE("serializing a leaf node is symmetric", "[serializing]") {
    // Making a map of ints to ints
    leafnode_ptr leaf = boost::make_shared<LeafNode>();
    leaf->insert(0, kv_pair(one_r, two_r));
    leaf->insert(1, kv_pair(two_r, one_r));
    memory serialized = SerializeNode(leaf);

    leafnode_ptr r = boost::dynamic_pointer_cast<LeafNode>(ParseNode(serialized, intToIntTree));
    REQUIRE(r->nodeType() == TYPE_LEAF);
    REQUIRE(r->pairCount() == 2);

    REQUIRE( rngcmp(r->pair(0).key, one_r) == 0 );
    REQUIRE( rngcmp(r->pair(0).value, two_r) == 0 );

    REQUIRE( rngcmp(r->pair(1).key, two_r) == 0 );
    REQUIRE( rngcmp(r->pair(1).value, one_r) == 0 );
}

TEST_CASE("serializing an internal node is symmetric", "[serializing]")
{
    // Making a map of ints to ints
    internalnode_ptr internal = boost::make_shared<InternalNode>();
    internal->insert(0, node_branch(one_r, 1, 1));
    internal->insert(1, node_branch(two_r, 2, 2));
    internal->insert(2, node_branch(three_r, 3, 3));
    memory serialized = SerializeNode(internal);

    internalnode_ptr r = boost::dynamic_pointer_cast<InternalNode>(ParseNode(serialized, intToIntTree));
    REQUIRE(r->nodeType() == TYPE_INTERNAL);
    REQUIRE(r->branchCount() == 3);

    /**/
    REQUIRE( r->branches[0].minKey.empty() );  // Exception
    REQUIRE( r->branches[0].nodeID == 1 );
    REQUIRE( r->branches[0].itemCount == 1 );

    /**/
    REQUIRE( rngcmp(r->branches[1].minKey, two_r) == 0 );
    REQUIRE( r->branches[1].nodeID == 2 );
    REQUIRE( r->branches[1].itemCount == 2 );

    /**/
    REQUIRE( rngcmp(r->branches[2].minKey, three_r) == 0 );
    REQUIRE( r->branches[2].nodeID == 3 );
    REQUIRE( r->branches[2].itemCount == 3 );
}

TEST_CASE("serializing an overflow node is symmetric", "[serializing]")
{
    overflownode_ptr overflow = boost::make_shared<OverflowNode>();
    overflow->append(one_r);
    overflow->append(two_r);
    memory serialized = SerializeNode(overflow);

    overflownode_ptr r = boost::dynamic_pointer_cast<OverflowNode>(ParseNode(serialized, intToIntTree));
    REQUIRE(r->nodeType() == TYPE_OVERFLOW);
    REQUIRE(r->valueCount() == 2);
    REQUIRE(r->values[0] == one_r);
    REQUIRE(r->values[1] == two_r);
}

TEST_CASE("calculating size of leaf node with different keys", "[serializing]")
{
    leafnode_ptr leaf = boost::make_shared<LeafNode>();
    for (unsigned i = 0; i < 128; i++)
        leaf->insert(i, kv_pair(intCopy(i), intCopy(i)));

    LeafNodeSize s(leaf, 1024);
    REQUIRE( s.splitIndex() == 64);
    // 64 * 8 == 512, which is half of the block size
    REQUIRE( s.overflowStart() == 64);
}

TEST_CASE("calculate overflow when postfix is all the same key", "[serializing]")
{
    leafnode_ptr leaf = boost::make_shared<LeafNode>();
    for (unsigned i = 0; i < 50; i++)
        leaf->insert(i, kv_pair(intCopy(i), intCopy(i)));
    for (unsigned i = 50; i < 128; i++)
        leaf->insert(i, kv_pair(intCopy(50), intCopy(i)));

    LeafNodeSize s(leaf, 1024);
    REQUIRE( s.overflowStart() == 51);
    // Put everything after 64 in the overflow block
    // 64 * 8 == 512, which is half of the block size
    REQUIRE( s.splitIndex() == 128);
}

TEST_CASE("calculate overflow when middle is the same key", "[serializing]")
{
    leafnode_ptr leaf = boost::make_shared<LeafNode>();
    for (unsigned i = 0; i < 50; i++)
        leaf->insert(i, kv_pair(intCopy(i), intCopy(i)));
    for (unsigned i = 50; i < 128; i++)
        leaf->insert(i, kv_pair(intCopy(50), intCopy(i)));
    leaf->insert(128, kv_pair(intCopy(51), intCopy(51)));

    LeafNodeSize s(leaf, 1024);
    REQUIRE( s.overflowStart() == 51);
    // Put everything after 64 in the overflow block
    // 64 * 8 == 512, which is half of the block size
    REQUIRE( s.splitIndex() == 128 );
}


TEST_CASE("calculating size of overflow node", "[serializing]")
{
    overflownode_ptr overflow = boost::make_shared<OverflowNode>();
    for (unsigned i = 0; i < 256; i++)
        overflow->append(intCopy(i));

    OverflowNodeSize s(overflow, 1024);
    REQUIRE( s.splitIndex() == 252 );
}
