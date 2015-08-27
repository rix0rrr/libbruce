#include <catch/catch.hpp>
#include <libbruce/bruce.h>
#include "serializing.h"

#include <stdio.h>

using namespace bruce;

/**
 * Return the size of a 32-bit int
 */
uint32_t intSize(const void *) { return sizeof(uint32_t); }

int rngcmp(const memory &a, const memory &b)
{
    int ret = memcmp(a.ptr(), b.ptr(), std::min(a.size(), b.size()));
    if (ret != 0) return ret;
    return a.size() - b.size();
}

tree_functions intToIntTree(NULL, &intSize, &intSize);

TEST_CASE("serializing a leaf node is symmetric", "[serializing]" ) {
    // Making a map of ints to ints
    uint32_t one = 1;
    uint32_t two = 2;
    memory one_r(&one, sizeof(one));
    memory two_r(&two, sizeof(two));

    leafnode_ptr leaf = boost::make_shared<LeafNode>();
    leaf->insert(0, kv_pair(one_r, two_r));
    leaf->insert(1, kv_pair(two_r, one_r));
    memory serialized = SerializeNode(leaf);

    leafnode_ptr r = boost::dynamic_pointer_cast<LeafNode>(ParseNode(serialized, intToIntTree));
    REQUIRE(r->isLeafNode());
    REQUIRE(r->count() == 2);

    REQUIRE( rngcmp(r->pair(0).key, one_r) == 0 );
    REQUIRE( rngcmp(r->pair(0).value, two_r) == 0 );

    REQUIRE( rngcmp(r->pair(1).key, two_r) == 0 );
    REQUIRE( rngcmp(r->pair(1).value, one_r) == 0 );
}

TEST_CASE("serializing an internal node is symmetric", "[serializing]" ) {
    // Making a map of ints to ints
    uint32_t one = 1;
    uint32_t two = 2;
    uint32_t three = 3;
    memory one_r(&one, sizeof(one));
    memory two_r(&two, sizeof(two));
    memory three_r(&three, sizeof(three));

    internalnode_ptr internal = boost::make_shared<InternalNode>();
    internal->insert(0, node_branch(one_r, 1, 1));
    internal->insert(1, node_branch(two_r, 2, 2));
    internal->insert(2, node_branch(three_r, 3, 3));
    memory serialized = SerializeNode(internal);

    internalnode_ptr r = boost::dynamic_pointer_cast<InternalNode>(ParseNode(serialized, intToIntTree));
    REQUIRE(!r->isLeafNode());
    REQUIRE(r->count() == 3);

    /**/
    REQUIRE( r->branches()[0].minKey.empty() );  // Exception
    REQUIRE( r->branches()[0].nodeId == 1 );
    REQUIRE( r->branches()[0].itemCount == 1 );

    /**/
    REQUIRE( rngcmp(r->branches()[1].minKey, two_r) == 0 );
    REQUIRE( r->branches()[1].nodeId == 2 );
    REQUIRE( r->branches()[1].itemCount == 2 );

    /**/
    REQUIRE( rngcmp(r->branches()[2].minKey, three_r) == 0 );
    REQUIRE( r->branches()[2].nodeId == 3 );
    REQUIRE( r->branches()[2].itemCount == 3 );

}
