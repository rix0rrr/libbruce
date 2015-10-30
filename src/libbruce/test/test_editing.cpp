#include <catch/catch.hpp>
#include <libbruce/bruce.h>

#include "tree_impl.h"
#include "testhelpers.h"
#include "serializing.h"

#include <stdio.h>

using namespace libbruce;

TEST_CASE("writing a new single leaf tree")
{
    be::mem mem(1024);
    tree_impl tree(mem, maybe_nodeid(), g_testPool, intToIntTree);
    tree.insert(one_r, two_r);

    mutation mut = tree.write();

    // Check that we wrote a page to storage, and that that page deserializes
    REQUIRE( mut.success() );
    REQUIRE( mut.createdIDs().size() == 1 );

    THEN("it can be deserialized to a leaf")
    {
        mempage page = mem.get(mut.createdIDs()[0]);
        leafnode_ptr r = boost::dynamic_pointer_cast<LeafNode>(ParseNode(page, intToIntTree));

        REQUIRE(r->pairCount() == 1);
        REQUIRE(rngcmp(r->get_at(0)->first, one_r) == 0);
        REQUIRE(rngcmp(r->get_at(0)->second, two_r) == 0);
    }

    WHEN("a new key is added to it")
    {
        tree_impl tree2(mem, mut.newRootID(), g_testPool, intToIntTree);
        tree2.insert(two_r, one_r);

        mutation mut2 = tree2.write();

        THEN("it can still be deserialized")
        {
            mempage page = mem.get(*mut2.newRootID());
            leafnode_ptr r = boost::dynamic_pointer_cast<LeafNode>(ParseNode(page, intToIntTree));

            REQUIRE(r->pairCount() == 2);
            REQUIRE(rngcmp(r->get_at(0)->first, one_r) == 0);
            REQUIRE(rngcmp(r->get_at(0)->second, two_r) == 0);
            REQUIRE(rngcmp(r->get_at(1)->first, two_r) == 0);
            REQUIRE(rngcmp(r->get_at(1)->second, one_r) == 0);
        }
    }
}

TEST_CASE("inserting a lot of keys leads to split nodes")
{
    be::mem mem(1024);
    tree_impl tree(mem, maybe_nodeid(), g_testPool, intToIntTree);

    for (uint32_t i = 0; i < 140; i++)
        tree.insert(intCopy(i), intCopy(i));

    mutation mut = tree.write();

    REQUIRE( mem.blockCount() == 3 );

    mempage rootPage = mem.get(*mut.newRootID());
    internalnode_ptr rootNode = boost::dynamic_pointer_cast<InternalNode>(ParseNode(rootPage, intToIntTree));

    REQUIRE( rootNode->branchCount() == 2 );
    REQUIRE( rootNode->itemCount() == 140 );

    mempage leftPage = mem.get(rootNode->branch(0).nodeID);
    leafnode_ptr leftNode = boost::dynamic_pointer_cast<LeafNode>(ParseNode(leftPage, intToIntTree));
    REQUIRE( leftNode->pairCount() == rootNode->branch(0).itemCount );

    mempage rightPage = mem.get(rootNode->branch(1).nodeID);
    leafnode_ptr rightNode = boost::dynamic_pointer_cast<LeafNode>(ParseNode(rightPage, intToIntTree));
    REQUIRE( rightNode->pairCount() == rootNode->branch(1).itemCount );

    REQUIRE( leftNode->itemCount() + rightNode->itemCount() == rootNode->itemCount() );
}

TEST_CASE("split is kosher")
{
    be::mem mem(1024);
    tree_impl tree(mem, maybe_nodeid(), g_testPool, intToIntTree);

    for (uint32_t i = 0; i < 128; i++)
        tree.insert(intCopy(i), intCopy(i));

    mutation mut = tree.write();

    mempage rootPage = mem.get(*mut.newRootID());
    internalnode_ptr rootNode = boost::dynamic_pointer_cast<InternalNode>(ParseNode(rootPage, intToIntTree));

    REQUIRE( rootNode->branchCount() == 2 );
    memslice splitKey = rootNode->branch(1).minKey;

    mempage leftPage = mem.get(rootNode->branch(0).nodeID);
    mempage rightPage = mem.get(rootNode->branch(1).nodeID);
    leafnode_ptr leftNode = boost::dynamic_pointer_cast<LeafNode>(ParseNode(leftPage, intToIntTree));
    leafnode_ptr rightNode = boost::dynamic_pointer_cast<LeafNode>(ParseNode(rightPage, intToIntTree));

    for (int i = 0; i < leftNode->pairCount(); i++)
    {
        REQUIRE( intCompare(leftNode->get_at(i)->first, splitKey) < 0 );
        if (i > 0)
            REQUIRE( intCompare(leftNode->get_at(i-1)->first, leftNode->get_at(i)->first) <= 0 );
    }

    for (int i = 0; i < rightNode->pairCount(); i++)
    {
        REQUIRE( intCompare(rightNode->get_at(i)->first, splitKey) >= 0 );
    }
}

TEST_CASE("inserting then deleting from a leaf")
{
    be::mem mem(1024);
    tree_impl tree(mem, maybe_nodeid(), g_testPool, intToIntTree);
    tree.insert(one_r, two_r);
    tree.remove(one_r, true);
    mutation mut = tree.write();

    THEN("it can be deserialized to an empty leaf")
    {
        mempage page = mem.get(*mut.newRootID());
        leafnode_ptr r = boost::dynamic_pointer_cast<LeafNode>(ParseNode(page, intToIntTree));

        REQUIRE(r->pairCount() == 0);
    }
}

TEST_CASE("inserting then deleting from an internal node")
{
    be::mem mem(1024);
    tree_impl tree(mem, maybe_nodeid(), g_testPool, intToIntTree);
    for (uint32_t i = 0; i < 128; i++)
        tree.insert(intCopy(i), intCopy(i));

    SECTION("deleting from the left")
    {
        uint32_t x = 40;
        tree.remove(memslice(&x, sizeof(x)), true);
        mutation mut = tree.write();

        mempage rootPage = mem.get(*mut.newRootID());
        internalnode_ptr rootNode = boost::dynamic_pointer_cast<InternalNode>(ParseNode(rootPage, intToIntTree));
        REQUIRE( rootNode->itemCount() == 127 );
    }

    SECTION("deleting from the right")
    {
        uint32_t x = 80;
        tree.remove(memslice(&x, sizeof(x)), true);
        mutation mut = tree.write();

        mempage rootPage = mem.get(*mut.newRootID());
        internalnode_ptr rootNode = boost::dynamic_pointer_cast<InternalNode>(ParseNode(rootPage, intToIntTree));
        REQUIRE( rootNode->itemCount() == 127 );
    }
}

TEST_CASE("inserting a bunch of values with the same key and selective removal works")
{
    be::mem mem(1024);
    tree_impl tree(mem, maybe_nodeid(), g_testPool, intToIntTree);
    for (uint32_t i = 0; i < 128; i++)
        tree.insert(two_r, intCopy(i));

    SECTION("deleting a low key & value")
    {
        tree.remove(two_r, intCopy(40), true);
        mutation mut = tree.write();

        mempage rootPage = mem.get(*mut.newRootID());
        leafnode_ptr rootNode = boost::dynamic_pointer_cast<LeafNode>(ParseNode(rootPage, intToIntTree));
        REQUIRE( rootNode->itemCount() == 127 );
    }

    SECTION("deleting a high key & value")
    {
        tree.remove(two_r, intCopy(80), true);
        mutation mut = tree.write();

        mempage rootPage = mem.get(*mut.newRootID());
        leafnode_ptr rootNode = boost::dynamic_pointer_cast<LeafNode>(ParseNode(rootPage, intToIntTree));
        REQUIRE( rootNode->itemCount() == 127 );
    }

    SECTION("deleting a nonexistent value")
    {
        tree.remove(two_r, intCopy(130), false);
        mutation mut = tree.write();

        mempage rootPage = mem.get(*mut.newRootID());
        leafnode_ptr rootNode = boost::dynamic_pointer_cast<LeafNode>(ParseNode(rootPage, intToIntTree));
        REQUIRE( rootNode->itemCount() == 128 );
    }
}

TEST_CASE("write new pages, delete old ones")
{
    maybe_nodeid treeID;
    be::mem mem(1024);
    {
        tree_impl tree(mem, maybe_nodeid(), g_testPool, intToIntTree);
        for (uint32_t i = 0; i < 128; i++)
            tree.insert(intCopy(i), intCopy(i));
        mutation mut = tree.write();
        REQUIRE( mem.blockCount() == 3 );
        treeID = mut.newRootID();
    }

    {
        tree_impl tree(mem, treeID, g_testPool, intToIntTree);
        tree.insert(intCopy(140), intCopy(140));
        mutation mut = tree.write();
        REQUIRE( mem.blockCount() == 5 );
        REQUIRE( mut.createdIDs().size() == 2 );
        REQUIRE( mut.obsoleteIDs().size() == 2 );
    }
}

TEST_CASE("postfix a whole bunch of the same keys into anode")
{
    be::mem mem(1024);
    tree_impl tree(mem, maybe_nodeid(), g_testPool, intToIntTree);
    for (unsigned i = 0; i < 50; i++)
        tree.insert(intCopy(i), intCopy(i));
    for (unsigned i = 50; i < 400; i++)
        tree.insert(intCopy(50), intCopy(i));
    mutation mut = tree.write();

    REQUIRE( mem.blockCount() == 3 ); // Expecting leaf and two overflows
}

TEST_CASE("int to int tree")
{
    be::mem mem(1024);
    tree<uint32_t, uint32_t> t(maybe_nodeid(), mem);

    t.insert(1, 1);
    t.insert(2, 2);
    mutation mut = t.write();

    mempage page = mem.get(*mut.newRootID());
    leafnode_ptr node = boost::dynamic_pointer_cast<LeafNode>(ParseNode(page, intToIntTree));

    REQUIRE( traits::convert<uint32_t>::from_bytes(node->get_at(0)->first) == 1 );
    REQUIRE( traits::convert<uint32_t>::from_bytes(node->get_at(0)->second) == 1 );
    REQUIRE( traits::convert<uint32_t>::from_bytes(node->get_at(1)->first) == 2 );
    REQUIRE( traits::convert<uint32_t>::from_bytes(node->get_at(1)->second) == 2 );
}

TEST_CASE("string to int tree")
{
    be::mem mem(1024);
    tree<std::string, uint32_t> t(maybe_nodeid(), mem);

    t.insert("one", 1);
    t.insert("two", 2);
    mutation mut = t.write();

    mempage page = mem.get(*mut.newRootID());
    leafnode_ptr node = boost::dynamic_pointer_cast<LeafNode>(ParseNode(page, tree<std::string, uint32_t>::fns));

    REQUIRE( traits::convert<std::string>::from_bytes(node->get_at(0)->first) == "one" );
    REQUIRE( traits::convert<uint32_t>::from_bytes(node->get_at(0)->second) == 1 );
    REQUIRE( traits::convert<std::string>::from_bytes(node->get_at(1)->first) == "two" );
    REQUIRE( traits::convert<uint32_t>::from_bytes(node->get_at(1)->second) == 2 );
}

TEST_CASE("in to string tree")
{
    be::mem mem(1024);
    tree<uint32_t, std::string> t(maybe_nodeid(), mem);

    t.insert(1, "one is one");
    t.insert(2, "two is two");
    mutation mut = t.write();

    mempage page = mem.get(*mut.newRootID());
    leafnode_ptr node = boost::dynamic_pointer_cast<LeafNode>(ParseNode(page, tree<uint32_t, std::string>::fns));

    REQUIRE( traits::convert<uint32_t>::from_bytes(node->get_at(0)->first) == 1 );
    REQUIRE( traits::convert<std::string>::from_bytes(node->get_at(0)->second) == "one is one" );
    REQUIRE( traits::convert<uint32_t>::from_bytes(node->get_at(1)->first) == 2 );
    REQUIRE( traits::convert<std::string>::from_bytes(node->get_at(1)->second) == "two is two" );
}

TEST_CASE("binary to string tree")
{
    be::mem mem(1024);
    tree<std::string, binary> t(maybe_nodeid(), mem);

    t.insert("one", binary("\x01\x00\x01", 3));
    t.insert("two", binary("\x02\x00\x02", 3));
    mutation mut = t.write();

    mempage page = mem.get(*mut.newRootID());
    leafnode_ptr node = boost::dynamic_pointer_cast<LeafNode>(ParseNode(page, tree<std::string, binary>::fns));
    memslice k = node->get_at(0)->first;
    REQUIRE( traits::convert<std::string>::from_bytes(node->get_at(0)->first) == "one" );
    REQUIRE( traits::convert<binary>::from_bytes(node->get_at(0)->second) == binary("\x01\x00\x01", 3) );
    REQUIRE( traits::convert<std::string>::from_bytes(node->get_at(1)->first) == "two" );
    REQUIRE( traits::convert<binary>::from_bytes(node->get_at(1)->second) == binary("\x02\x00\x02", 3) );
}

TEST_CASE("test inserting into overflow block", "[nodes]")
{
    be::mem mem(1024);
    tree<int, int> t(maybe_nodeid(), mem);

    // This should produce a leaf node with 1 key and 2 overflow blocks
    for (int i = 0; i < 300; i++)
        t.insert(0, i);

    mutation mut = t.write();

    REQUIRE( mem.blockCount() == 3 );

}

TEST_CASE("remove branch when empty")
{
    be::mem mem(1024);

    // GIVEN
    put_result root = make_internal()
        .brn(make_leaf(intToIntTree)
           .kv(1, 1)
           .put(mem)) // 0
        .brn(make_leaf(intToIntTree)
           .kv(2, 2)
           .put(mem)) // 1
        .put(mem); // 2
    tree<int, int> edit(root.nodeID, mem);

    // WHEN
    edit.remove(1, true);
    mutation mut = edit.write();

    // THEN
    mempage page = mem.get(*mut.newRootID());
    internalnode_ptr internal = boost::dynamic_pointer_cast<InternalNode>(ParseNode(page, tree<int, int>::fns));

    REQUIRE( internal->branchCount() == 1 );

    REQUIRE( mut.obsoleteIDs().size() == 2 );
    REQUIRE( mut.createdIDs().size() == 1 );
}

TEST_CASE("upserts")
{
    be::mem mem(1024);

    // GIVEN
    put_result root = make_internal()
        .brn(make_leaf(intToIntTree)
           .kv(1, 1)
           .put(mem)) // 0
        .brn(make_leaf(intToIntTree)
           .kv(3, 3)
           .put(mem)) // 1
        .put(mem); // 2
    tree<int, int> edit(root.nodeID, mem);

    SECTION("upsert becomes an update")
    {
        edit.upsert(1, 2, true);
        mutation mut = edit.write();

        tree<int, int> query(*mut.newRootID(), mem);
        REQUIRE( query.find(1).value() == 2 );

        // Check rank as well
        REQUIRE( query.find(3).rank() == 1 );
        REQUIRE( query.seek(1).key() == 3 );
    }

    SECTION("upsert becomes an insert")
    {
        edit.upsert(2, 2, true);
        mutation mut = edit.write();

        tree<int, int> query(*mut.newRootID(), mem);
        REQUIRE( query.find(1).value() == 1 );
        REQUIRE( query.find(2).value() == 2 );

        // Check rank as well
        REQUIRE( query.find(3).rank() == 2 );
        REQUIRE( query.seek(2).key() == 3 );
    }
}


TEST_CASE("inserting after overflow node")
{
    be::mem mem(1024);

    // GIVEN
    put_result root = make_leaf(intToIntTree)
           .kv(1, 1)
           .kv(3, 3)
           .overflow(make_overflow()
                .val(4)
                .val(5)
                .put(mem))
           .put(mem);
    tree<int, int> edit(root.nodeID, mem);

    // WHEN
    edit.insert(4, 4);
    mutation mut = edit.write();

    // THEN
    leafnode_ptr node = loadLeaf(mem, *mut.newRootID());
    REQUIRE( node->pairs.size() == 5);
}

TEST_CASE("inserting after overflow node too big to pull in")
{
    be::mem mem(100);

    // GIVEN
    put_result root = make_leaf(intToIntTree)
           .kv(1, 1)
           .kv(2, 2)
           .kv(3, 3)
           .overflow(make_overflow()
                .val(4)
                .val(5)
                .val(6)
                .val(7)
                .val(8)
                .val(9)
                .val(10)
                .val(11)
                .put(mem))
           .put(mem);
    tree<int, int> edit(root.nodeID, mem);

    // WHEN
    edit.insert(4, 4);
    mutation mut = edit.write();

    // THEN
    internalnode_ptr internal = loadInternal(mem, *mut.newRootID());
    leafnode_ptr left = loadLeaf(mem, internal->branches[0].nodeID);
    leafnode_ptr right = loadLeaf(mem, internal->branches[1].nodeID);

    REQUIRE(left->pairs.size() == 3);
    REQUIRE(!left->overflow.empty());
    REQUIRE(right->pairs.size() == 1);
}

TEST_CASE("root has to split because its too large")
{
    be::mem mem(64);

    // GIVEN
    put_result root = make_leaf(intToIntTree)
           .kv(1, 1)
           .kv(3, 3)
           .overflow(make_overflow()
                .val(4)
                .val(5)
                .val(6)
                .val(7)
                .val(8)
                .put(mem))
           .put(mem);
    tree<int, int> edit(root.nodeID, mem);

    // WHEN
    edit.insert(4, 4);
    mutation mut = edit.write();

    // THEN
    internalnode_ptr internal = loadInternal(mem, *mut.newRootID());
    internalnode_ptr int1 = loadInternal(mem, internal->branches[0].nodeID);
    internalnode_ptr int2 = loadInternal(mem, internal->branches[1].nodeID);
}
