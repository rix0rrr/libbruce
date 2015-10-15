#include <catch/catch.hpp>
#include "testhelpers.h"
#include "serializing.h"
#include <libbruce/bruce.h>

#include <stdio.h>

using namespace libbruce;

TEST_CASE("prefix and postfix increment")
{
    be::mem mem(1024);
    put_result root = make_leaf(intToIntTree)
        .kv(1, 1)
        .kv(3, 3)
        .put(mem);
    query_tree<uint32_t, uint32_t> query(root.nodeID, mem);

    query_tree<uint32_t, uint32_t>::iterator it = query.find(1);
    REQUIRE(it++.value() == 1);

    it = query.find(1);
    REQUIRE((++it).value() == 3);
}

TEST_CASE("arbitrary increment")
{
    be::mem mem(1024);
    put_result root = make_leaf(intToIntTree)
        .kv(1, 1)
        .kv(3, 3)
        .kv(5, 5)
        .kv(7, 7)
        .put(mem);
    query_tree<uint32_t, uint32_t> query(root.nodeID, mem);

    query_tree<uint32_t, uint32_t>::iterator it = query.find(1);
    it += 2;

    REQUIRE(it.value() == 5);

    it += -2;
    REQUIRE(it.value() == 1);
}

TEST_CASE("iterator for an empty tree")
{
    be::mem mem(1024);
    put_result root = make_leaf(intToIntTree)
        .put(mem);
    query_tree<uint32_t, uint32_t> query(root.nodeID, mem);

    query_tree<uint32_t, uint32_t>::iterator it = query.begin();
    REQUIRE( !it );
}

TEST_CASE("iterator compare")
{
    be::mem mem(1024);
    put_result root = make_leaf(intToIntTree)
        .put(mem);
    query_tree<uint32_t, uint32_t> query(root.nodeID, mem);

    REQUIRE( query.begin() == query.end() );
    REQUIRE( query.end() == query.end() );
}

TEST_CASE("iteration crossing between leafs")
{
    be::mem mem(1024);

    put_result root = make_internal()
        .brn(make_leaf(intToIntTree)
           .kv(1, 1).kv(3, 3).put(mem))
        .brn(make_leaf(intToIntTree)
           .kv(5, 5).kv(7, 7).put(mem))
        .put(mem);

    query_tree<uint32_t, uint32_t> query(root.nodeID, mem);

    SECTION("iterate over all")
    {
        query_tree<uint32_t, uint32_t>::iterator it = query.find(1);
        REQUIRE( it );
        REQUIRE( it++.value() == 1 );
        REQUIRE( it++.value() == 3 );
        REQUIRE( it++.value() == 5 );
        REQUIRE( it++.value() == 7 );
        REQUIRE( !it );
    }
}

TEST_CASE("iteration across overflow nodes")
{
    be::mem mem(1024);

    put_result root = make_internal()
        .brn(make_leaf(intToIntTree)
           .kv(1, 1)
           .kv(3, 3)
           .overflow(make_overflow()
                .val(30)
                .next(make_overflow()
                    .val(31)
                    .put(mem))
                .put(mem))
           .put(mem))
        .brn(make_leaf(intToIntTree)
           .kv(5, 5)
           .kv(7, 7)
           .put(mem))
        .put(mem);

    query_tree<uint32_t, uint32_t> query(root.nodeID, mem);

    SECTION("iterate over all")
    {
        query_tree<uint32_t, uint32_t>::iterator it = query.find(1);
        REQUIRE( it );
        REQUIRE( it++.value() == 1 );
        REQUIRE( it++.value() == 3 );
        REQUIRE( it++.value() == 30 );
        REQUIRE( it++.value() == 31 );
        REQUIRE( it++.value() == 5 );
        REQUIRE( it++.value() == 7 );
        REQUIRE( !it );
    }
}

TEST_CASE("iteration ends after overflow")
{
    be::mem mem(1024);

    put_result root = make_leaf(intToIntTree)
        .kv(1, 1)
        .kv(3, 3)
        .overflow(make_overflow()
            .val(30)
            .put(mem))
        .put(mem);

    query_tree<uint32_t, uint32_t> query(root.nodeID, mem);

    SECTION("iterate over all")
    {
        query_tree<uint32_t, uint32_t>::iterator it = query.find(1);
        REQUIRE( it );
        REQUIRE( it++.value() == 1 );
        REQUIRE( it++.value() == 3 );
        REQUIRE( it++.value() == 30 );
        REQUIRE( !it );
    }
}

TEST_CASE("tree 2 levels deep")
{
    be::mem mem(1024);

    put_result root = make_internal()
        .brn(make_internal()
             .brn(make_leaf(intToIntTree)
                 .kv(1, 1)
                 .put(mem))
             .brn(make_leaf(intToIntTree)
                 .kv(3, 3)
                 .put(mem))
             .put(mem))
        .brn(make_internal()
             .brn(make_leaf(intToIntTree)
                 .kv(5, 5)
                 .put(mem))
             .brn(make_leaf(intToIntTree)
                 .kv(7, 7)
                 .put(mem))
             .put(mem))
        .put(mem);

    query_tree<uint32_t, uint32_t> query(root.nodeID, mem);

    SECTION("iterate over all")
    {
        query_tree<uint32_t, uint32_t>::iterator it = query.find(1);
        REQUIRE( it );
        REQUIRE( it++.value() == 1 );
        REQUIRE( it++.value() == 3 );
        REQUIRE( it++.value() == 5 );
        REQUIRE( it++.value() == 7 );
        REQUIRE( !it );
    }
}

TEST_CASE("iteration with queued insert")
{
    be::mem mem(1024);

    put_result root = make_internal()
        .brn(make_leaf(intToIntTree)
           .kv(1, 1).kv(3, 3).put(mem))
        .brn(make_leaf(intToIntTree)
           .kv(5, 5).kv(7, 7).put(mem))
        .put(mem);

    query_tree<uint32_t, uint32_t> query(root.nodeID, mem);

    SECTION("in first leaf")
    {
        query.queue_insert(4, 4);

        query_tree<uint32_t, uint32_t>::iterator it = query.find(1);
        REQUIRE( it++.value() == 1 );
        REQUIRE( it++.value() == 3 );
        REQUIRE( it++.value() == 4 );
        REQUIRE( it++.value() == 5 );
        REQUIRE( it++.value() == 7 );
        REQUIRE( !it );
    }

    SECTION("in second leaf")
    {
        query.queue_insert(6, 6);

        query_tree<uint32_t, uint32_t>::iterator it = query.find(1);
        REQUIRE( it++.value() == 1 );
        REQUIRE( it++.value() == 3 );
        REQUIRE( it++.value() == 5 );
        REQUIRE( it++.value() == 6 );
        REQUIRE( it++.value() == 7 );
        REQUIRE( !it );
    }
}


TEST_CASE("iteration with queued delete")
{
    be::mem mem(1024);

    put_result root = make_internal()
        .brn(make_leaf(intToIntTree)
           .kv(1, 1).kv(3, 3).put(mem))
        .brn(make_leaf(intToIntTree)
           .kv(5, 5).kv(7, 7).put(mem))
        .put(mem);

    query_tree<uint32_t, uint32_t> query(root.nodeID, mem);

    SECTION("in first leaf")
    {
        query.queue_remove(3, false);

        query_tree<uint32_t, uint32_t>::iterator it = query.find(1);
        REQUIRE( it++.value() == 1 );
        REQUIRE( it++.value() == 5 );
        REQUIRE( it++.value() == 7 );
        REQUIRE( !it );
    }

    SECTION("in second leaf")
    {
        query.queue_remove(5, false);

        query_tree<uint32_t, uint32_t>::iterator it = query.find(1);
        REQUIRE( it++.value() == 1 );
        REQUIRE( it++.value() == 3 );
        REQUIRE( it++.value() == 7 );
        REQUIRE( !it );
    }
}

// Rank stuff (with guaranteed and unguaranteed queued inserts)
