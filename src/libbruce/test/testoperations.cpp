#include <catch/catch.hpp>
#include <libbruce/bruce.h>

#include "operations.h"
#include "testhelpers.h"
#include "serializing.h"

#include <stdio.h>

using namespace bruce;

TEST_CASE("writing a new single leaf tree")
{
    be::mem mem(1024);
    mutable_tree tree(mem, maybe_nodeid(), intToIntTree);

    tree.insert(one_r, two_r);

    mutation mut = tree.flush();

    // Check that we wrote a page to storage, and that that page deserializes
    REQUIRE( mut.success() );
    REQUIRE( mut.createdIDs().size() == 1 );
    REQUIRE( mut.createdIDs()[0] == 0 );

    THEN("it can be deserialized to a leaf")
    {
        memory page = mem.get(0);
        leafnode_ptr r = boost::dynamic_pointer_cast<LeafNode>(ParseNode(page, intToIntTree));

        REQUIRE(r->count() == 1);
        REQUIRE(rngcmp(r->pair(0).key, one_r) == 0);
        REQUIRE(rngcmp(r->pair(0).value, two_r) == 0);
    }

    WHEN("a new key is added to it")
    {
        mutable_tree tree2(mem, mut.newRootID(), intToIntTree);
        tree2.insert(two_r, one_r);

        mutation mut2 = tree2.flush();

        THEN("it can still be deserialized")
        {
            memory page = mem.get(*mut2.newRootID());
            leafnode_ptr r = boost::dynamic_pointer_cast<LeafNode>(ParseNode(page, intToIntTree));

            REQUIRE(r->count() == 2);
            REQUIRE(rngcmp(r->pair(0).key, one_r) == 0);
            REQUIRE(rngcmp(r->pair(0).value, two_r) == 0);
            REQUIRE(rngcmp(r->pair(1).key, two_r) == 0);
            REQUIRE(rngcmp(r->pair(1).value, one_r) == 0);
        }
    }
}

TEST_CASE("inserting a lot of keys leads to split nodes")
{
    be::mem mem(1024);
    mutable_tree tree(mem, maybe_nodeid(), intToIntTree);

    for (uint32_t i = 0; i < 140; i++)
        tree.insert(memory(&i, sizeof(i)), memory(&i, sizeof(i)));

    mutation mut = tree.flush();

    REQUIRE( mem.blockCount() == 3 );

    memory rootPage = mem.get(*mut.newRootID());
    internalnode_ptr rootNode = boost::dynamic_pointer_cast<InternalNode>(ParseNode(rootPage, intToIntTree));

    REQUIRE( rootNode->count() == 2 );
    REQUIRE( rootNode->itemCount() == 140 );

    memory leftPage = mem.get(rootNode->branch(0).nodeID);
    leafnode_ptr leftNode = boost::dynamic_pointer_cast<LeafNode>(ParseNode(leftPage, intToIntTree));
    REQUIRE( leftNode->count() == rootNode->branch(0).itemCount );

    memory rightPage = mem.get(rootNode->branch(1).nodeID);
    leafnode_ptr rightNode = boost::dynamic_pointer_cast<LeafNode>(ParseNode(rightPage, intToIntTree));
    REQUIRE( rightNode->count() == rootNode->branch(1).itemCount );

    REQUIRE( leftNode->itemCount() + rightNode->itemCount() == rootNode->itemCount() );
}
