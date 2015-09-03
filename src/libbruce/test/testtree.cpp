/**
 * Testing the typesafe interface
 */
#include <catch/catch.hpp>
#include "testhelpers.h"
#include "serializing.h"
#include <libbruce/bruce.h>

#include <stdio.h>

using namespace bruce;

TEST_CASE("int to int tree")
{
    be::mem mem(1024);
    tree<uint32_t, uint32_t> t(maybe_nodeid(), mem);

    t.insert(1, 1);
    t.insert(2, 2);
    mutation mut = t.flush();

    memory page = mem.get(*mut.newRootID());
    leafnode_ptr node = boost::dynamic_pointer_cast<LeafNode>(ParseNode(page, intToIntTree));
    memory k = node->pair(0).key;
    REQUIRE( traits::convert<uint32_t>::from_bytes(node->pair(0).key) == 1 );
    REQUIRE( traits::convert<uint32_t>::from_bytes(node->pair(0).value) == 1 );
    REQUIRE( traits::convert<uint32_t>::from_bytes(node->pair(1).key) == 2 );
    REQUIRE( traits::convert<uint32_t>::from_bytes(node->pair(1).value) == 2 );
}

TEST_CASE("string to int tree")
{
    be::mem mem(1024);
    tree<std::string, uint32_t> t(maybe_nodeid(), mem);

    t.insert("one", 1);
    t.insert("two", 2);
    mutation mut = t.flush();

    memory page = mem.get(*mut.newRootID());
    leafnode_ptr node = boost::dynamic_pointer_cast<LeafNode>(ParseNode(page, tree<std::string, uint32_t>::fns()));
    memory k = node->pair(0).key;
    REQUIRE( traits::convert<std::string>::from_bytes(node->pair(0).key) == "one" );
    REQUIRE( traits::convert<uint32_t>::from_bytes(node->pair(0).value) == 1 );
    REQUIRE( traits::convert<std::string>::from_bytes(node->pair(1).key) == "two" );
    REQUIRE( traits::convert<uint32_t>::from_bytes(node->pair(1).value) == 2 );
}

TEST_CASE("in to string tree")
{
    be::mem mem(1024);
    tree<uint32_t, std::string> t(maybe_nodeid(), mem);

    t.insert(1, "one is one");
    t.insert(2, "two is two");
    mutation mut = t.flush();

    memory page = mem.get(*mut.newRootID());
    leafnode_ptr node = boost::dynamic_pointer_cast<LeafNode>(ParseNode(page, tree<uint32_t, std::string>::fns()));
    memory k = node->pair(0).key;
    REQUIRE( traits::convert<uint32_t>::from_bytes(node->pair(0).key) == 1 );
    REQUIRE( traits::convert<std::string>::from_bytes(node->pair(0).value) == "one is one" );
    REQUIRE( traits::convert<uint32_t>::from_bytes(node->pair(1).key) == 2 );
    REQUIRE( traits::convert<std::string>::from_bytes(node->pair(1).value) == "two is two" );
}

TEST_CASE("binary to string tree")
{
    be::mem mem(1024);
    tree<std::string, binary> t(maybe_nodeid(), mem);

    t.insert("one", binary("\x01\x00\x01", 3));
    t.insert("two", binary("\x02\x00\x02", 3));
    mutation mut = t.flush();

    memory page = mem.get(*mut.newRootID());
    leafnode_ptr node = boost::dynamic_pointer_cast<LeafNode>(ParseNode(page, tree<std::string, binary>::fns()));
    memory k = node->pair(0).key;
    REQUIRE( traits::convert<std::string>::from_bytes(node->pair(0).key) == "one" );
    REQUIRE( traits::convert<binary>::from_bytes(node->pair(0).value) == binary("\x01\x00\x01", 3) );
    REQUIRE( traits::convert<std::string>::from_bytes(node->pair(1).key) == "two" );
    REQUIRE( traits::convert<binary>::from_bytes(node->pair(1).value) == binary("\x02\x00\x02", 3) );
}
