#include <catch/catch.hpp>
#include <libbruce/bruce.h>

#include "edit_tree_impl.h"
#include "testhelpers.h"
#include "serializing.h"

#include <stdio.h>

using namespace bruce;

TEST_CASE("writing a new single leaf tree")
{
    be::mem mem(1024);
    edit_tree_impl tree(mem, maybe_nodeid(), intToIntTree);
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
        edit_tree_impl tree2(mem, mut.newRootID(), intToIntTree);
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
    edit_tree_impl tree(mem, maybe_nodeid(), intToIntTree);

    for (uint32_t i = 0; i < 140; i++)
        tree.insert(intCopy(i), intCopy(i));

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

TEST_CASE("split is kosher")
{
    be::mem mem(1024);
    edit_tree_impl tree(mem, maybe_nodeid(), intToIntTree);

    for (uint32_t i = 0; i < 128; i++)
        tree.insert(intCopy(i), intCopy(i));

    mutation mut = tree.flush();

    memory rootPage = mem.get(*mut.newRootID());
    internalnode_ptr rootNode = boost::dynamic_pointer_cast<InternalNode>(ParseNode(rootPage, intToIntTree));

    REQUIRE( rootNode->count() == 2 );
    memory splitKey = rootNode->branch(1).minKey;

    memory leftPage = mem.get(rootNode->branch(0).nodeID);
    memory rightPage = mem.get(rootNode->branch(1).nodeID);
    leafnode_ptr leftNode = boost::dynamic_pointer_cast<LeafNode>(ParseNode(leftPage, intToIntTree));
    leafnode_ptr rightNode = boost::dynamic_pointer_cast<LeafNode>(ParseNode(rightPage, intToIntTree));

    for (int i = 0; i < leftNode->count(); i++)
    {
        REQUIRE( intCompare(leftNode->pair(i).key, splitKey) < 0 );
        if (i > 0)
            REQUIRE( intCompare(leftNode->pair(i-1).key, leftNode->pair(i).key) <= 0 );
    }

    for (int i = 0; i < rightNode->count(); i++)
    {
        REQUIRE( intCompare(rightNode->pair(i).key, splitKey) >= 0 );
    }
}

TEST_CASE("inserting then deleting from a leaf")
{
    be::mem mem(1024);
    edit_tree_impl tree(mem, maybe_nodeid(), intToIntTree);
    tree.insert(one_r, two_r);
    tree.remove(one_r);
    mutation mut = tree.flush();

    THEN("it can be deserialized to an empty leaf")
    {
        memory page = mem.get(*mut.newRootID());
        leafnode_ptr r = boost::dynamic_pointer_cast<LeafNode>(ParseNode(page, intToIntTree));

        REQUIRE(r->count() == 0);
    }
}

TEST_CASE("inserting then deleting from an internal node")
{
    be::mem mem(1024);
    edit_tree_impl tree(mem, maybe_nodeid(), intToIntTree);
    for (uint32_t i = 0; i < 128; i++)
        tree.insert(intCopy(i), intCopy(i));

    SECTION("deleting from the left")
    {
        uint32_t x = 40;
        bool success = tree.remove(memory(&x, sizeof(x)));
        REQUIRE(success);

        mutation mut = tree.flush();

        memory rootPage = mem.get(*mut.newRootID());
        internalnode_ptr rootNode = boost::dynamic_pointer_cast<InternalNode>(ParseNode(rootPage, intToIntTree));
        REQUIRE( rootNode->itemCount() == 127 );
    }

    SECTION("deleting from the right")
    {
        uint32_t x = 80;
        bool success = tree.remove(memory(&x, sizeof(x)));
        REQUIRE(success);
        mutation mut = tree.flush();

        memory rootPage = mem.get(*mut.newRootID());
        internalnode_ptr rootNode = boost::dynamic_pointer_cast<InternalNode>(ParseNode(rootPage, intToIntTree));
        REQUIRE( rootNode->itemCount() == 127 );
    }
}

TEST_CASE("inserting a bunch of values with the same key and selective removal works")
{
    be::mem mem(1024);
    edit_tree_impl tree(mem, maybe_nodeid(), intToIntTree);
    for (uint32_t i = 0; i < 128; i++)
        tree.insert(two_r, intCopy(i));

    SECTION("deleting a low key & value")
    {
        REQUIRE( tree.remove(two_r, intCopy(40)) );
        mutation mut = tree.flush();

        memory rootPage = mem.get(*mut.newRootID());
        internalnode_ptr rootNode = boost::dynamic_pointer_cast<InternalNode>(ParseNode(rootPage, intToIntTree));
        REQUIRE( rootNode->itemCount() == 127 );
    }

    SECTION("deleting a high key & value")
    {
        REQUIRE( tree.remove(two_r, intCopy(80)) );
        mutation mut = tree.flush();

        memory rootPage = mem.get(*mut.newRootID());
        internalnode_ptr rootNode = boost::dynamic_pointer_cast<InternalNode>(ParseNode(rootPage, intToIntTree));
        REQUIRE( rootNode->itemCount() == 127 );
    }

    SECTION("deleting a nonexistent value")
    {
        REQUIRE( !tree.remove(two_r, intCopy(130)) );
        mutation mut = tree.flush();

        memory rootPage = mem.get(*mut.newRootID());
        internalnode_ptr rootNode = boost::dynamic_pointer_cast<InternalNode>(ParseNode(rootPage, intToIntTree));
        REQUIRE( rootNode->itemCount() == 128 );
    }
}

TEST_CASE("write new pages, delete old ones")
{
    maybe_nodeid treeID;
    be::mem mem(1024);
    {
        edit_tree_impl tree(mem, maybe_nodeid(), intToIntTree);
        for (uint32_t i = 0; i < 128; i++)
            tree.insert(intCopy(i), intCopy(i));
        mutation mut = tree.flush();
        REQUIRE( mem.blockCount() == 3 );
        treeID = mut.newRootID();
    }

    {
        edit_tree_impl tree(mem, treeID, intToIntTree);
        tree.insert(intCopy(140), intCopy(140));
        mutation mut = tree.flush();
        REQUIRE( mem.blockCount() == 5 );
        REQUIRE( mut.createdIDs().size() == 2 );
        REQUIRE( mut.obsoleteIDs().size() == 2 );

    }
}

/*
TEST_CASE("no writeback if no changes")
{
    be::mem mem(1024);
    edit_tree_impl tree(mem, maybe_nodeid(), intToIntTree);
    for (uint32_t i = 0; i < 128; i++)
        tree.insert(intCopy(i), intCopy(i));
    mutation mut = tree.flush();
    uint32_t blockCount1 = mem.blockCount();

    edit_tree_impl tree2(mem, mut.newRootID(), intToIntTree);
    tree2.remove(intCopy(140));
    mutation mut2 = tree2.flush();
    uint32_t blockCount2 = mem.blockCount();

    REQUIRE( blockCount1 == blockCount2 );
}
*/
