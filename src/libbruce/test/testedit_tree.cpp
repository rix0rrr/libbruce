/**
 * Testing the typesafe interface
 */
#include <catch/catch.hpp>
#include "testhelpers.h"
#include "serializing.h"
#include <libbruce/bruce.h>

#include <stdio.h>

using namespace libbruce;

TEST_CASE("int to int tree")
{
    be::mem mem(1024);
    edit_tree<uint32_t, uint32_t> t(maybe_nodeid(), mem);

    t.insert(1, 1);
    t.insert(2, 2);
    mutation mut = t.flush();

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
    edit_tree<std::string, uint32_t> t(maybe_nodeid(), mem);

    t.insert("one", 1);
    t.insert("two", 2);
    mutation mut = t.flush();

    mempage page = mem.get(*mut.newRootID());
    leafnode_ptr node = boost::dynamic_pointer_cast<LeafNode>(ParseNode(page, edit_tree<std::string, uint32_t>::fns));

    REQUIRE( traits::convert<std::string>::from_bytes(node->get_at(0)->first) == "one" );
    REQUIRE( traits::convert<uint32_t>::from_bytes(node->get_at(0)->second) == 1 );
    REQUIRE( traits::convert<std::string>::from_bytes(node->get_at(1)->first) == "two" );
    REQUIRE( traits::convert<uint32_t>::from_bytes(node->get_at(1)->second) == 2 );
}

TEST_CASE("in to string tree")
{
    be::mem mem(1024);
    edit_tree<uint32_t, std::string> t(maybe_nodeid(), mem);

    t.insert(1, "one is one");
    t.insert(2, "two is two");
    mutation mut = t.flush();

    mempage page = mem.get(*mut.newRootID());
    leafnode_ptr node = boost::dynamic_pointer_cast<LeafNode>(ParseNode(page, edit_tree<uint32_t, std::string>::fns));

    REQUIRE( traits::convert<uint32_t>::from_bytes(node->get_at(0)->first) == 1 );
    REQUIRE( traits::convert<std::string>::from_bytes(node->get_at(0)->second) == "one is one" );
    REQUIRE( traits::convert<uint32_t>::from_bytes(node->get_at(1)->first) == 2 );
    REQUIRE( traits::convert<std::string>::from_bytes(node->get_at(1)->second) == "two is two" );
}

TEST_CASE("binary to string tree")
{
    be::mem mem(1024);
    edit_tree<std::string, binary> t(maybe_nodeid(), mem);

    t.insert("one", binary("\x01\x00\x01", 3));
    t.insert("two", binary("\x02\x00\x02", 3));
    mutation mut = t.flush();

    mempage page = mem.get(*mut.newRootID());
    leafnode_ptr node = boost::dynamic_pointer_cast<LeafNode>(ParseNode(page, edit_tree<std::string, binary>::fns));
    memslice k = node->get_at(0)->first;
    REQUIRE( traits::convert<std::string>::from_bytes(node->get_at(0)->first) == "one" );
    REQUIRE( traits::convert<binary>::from_bytes(node->get_at(0)->second) == binary("\x01\x00\x01", 3) );
    REQUIRE( traits::convert<std::string>::from_bytes(node->get_at(1)->first) == "two" );
    REQUIRE( traits::convert<binary>::from_bytes(node->get_at(1)->second) == binary("\x02\x00\x02", 3) );
}

TEST_CASE("test inserting into overflow block", "[nodes]")
{
    be::mem mem(1024);
    edit_tree<int, int> t(maybe_nodeid(), mem);

    // This should produce a leaf node with 1 key and 2 overflow blocks
    for (int i = 0; i < 300; i++)
        t.insert(0, i);

    mutation mut = t.flush();

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
    edit_tree<int, int> edit(root.nodeID, mem);

    // WHEN
    edit.remove(1);
    mutation mut = edit.flush();

    // THEN
    mempage page = mem.get(*mut.newRootID());
    internalnode_ptr internal = boost::dynamic_pointer_cast<InternalNode>(ParseNode(page, edit_tree<int, int>::fns));

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
    edit_tree<int, int> edit(root.nodeID, mem);

    SECTION("upsert becomes an update")
    {
        edit.upsert(1, 2);
        mutation mut = edit.flush();

        query_tree<int, int> query(*mut.newRootID(), mem);
        REQUIRE( query.find(1).value() == 2 );

        // Check rank as well
        REQUIRE( query.find(3).rank() == 1 );
        REQUIRE( query.seek(1).key() == 3 );
    }

    SECTION("upsert becomes an insert")
    {
        edit.upsert(2, 2);
        mutation mut = edit.flush();

        query_tree<int, int> query(*mut.newRootID(), mem);
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
    edit_tree<int, int> edit(root.nodeID, mem);

    // WHEN
    edit.insert(4, 4);
    mutation mut = edit.flush();

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
    edit_tree<int, int> edit(root.nodeID, mem);

    // WHEN
    edit.insert(4, 4);
    mutation mut = edit.flush();

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
    be::mem mem(60);

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
    edit_tree<int, int> edit(root.nodeID, mem);

    // WHEN
    edit.insert(4, 4);
    mutation mut = edit.flush();

    // THEN
    internalnode_ptr internal = loadInternal(mem, *mut.newRootID());
    internalnode_ptr int1 = loadInternal(mem, internal->branches[0].nodeID);
    internalnode_ptr int2 = loadInternal(mem, internal->branches[1].nodeID);
}
