#include <catch/catch.hpp>
#include <libbruce/bruce.h>

#include "query_tree_impl.h"
#include "testhelpers.h"

#include <stdio.h>

using namespace libbruce;

TEST_CASE("reading from a tree", "[query]")
{
    be::mem mem(1024);
    edit_tree<int, int> t(maybe_nodeid(), mem);
    t.insert(1, 1);
    t.insert(2, 2);
    mutation mut = t.flush();

    query_tree<int, int> q(*mut.newRootID(), mem);
    REQUIRE( *q.get(1) == 1 );
    REQUIRE( *q.get(2) == 2 );
    REQUIRE( !q.get(3) );
}

TEST_CASE("reading from a tree with queued insert", "[query]")
{
    be::mem mem(1024);
    edit_tree<int, int> t(maybe_nodeid(), mem);
    t.insert(0, 0);
    mutation mut = t.flush();

    query_tree<int, int> q(*mut.newRootID(), mem);
    q.queue_insert(1, 1);
    REQUIRE( *q.get(1) == 1 );
}

TEST_CASE("reading from a tree with queued remove", "[query]")
{
    be::mem mem(1024);
    edit_tree<int, int> t(maybe_nodeid(), mem);
    t.insert(1, 1);
    mutation mut = t.flush();

    query_tree<int, int> q(*mut.newRootID(), mem);
    q.queue_remove(1, true);
    REQUIRE( !q.get(1) );
}

TEST_CASE("reading from a tree with queued remove and insert", "[query]")
{
    be::mem mem(1024);
    edit_tree<int, int> t(maybe_nodeid(), mem);
    t.insert(0, 0);
    mutation mut = t.flush();

    query_tree<int, int> q(*mut.newRootID(), mem);
    q.queue_remove(1, true);
    q.queue_insert(1, 1);
    REQUIRE( *q.get(1) == 1 );
}

TEST_CASE("reading from a tree with queued insert and remove", "[query]")
{
    be::mem mem(1024);
    edit_tree<int, int> t(maybe_nodeid(), mem);
    t.insert(0, 0);
    mutation mut = t.flush();

    query_tree<int, int> q(*mut.newRootID(), mem);
    q.queue_insert(1, 1);
    q.queue_remove(1, true);
    REQUIRE( !q.get(1) );
}

TEST_CASE("seeking in a plain tree", "[query][rank]")
{
    be::mem mem(1024);
    put_result root = make_internal()
        .brn(make_leaf()
           .kv(1, 1).kv(3, 3).put(mem))
        .brn(make_leaf()
           .kv(5, 5).kv(7, 7).put(mem))
        .put(mem);
    query_tree<uint32_t, uint32_t> query(root.nodeID, mem);

    query_tree<uint32_t, uint32_t>::iterator it;
    it = query.seek(0);
    REQUIRE(it.value() == 1);

    it = query.seek(1);
    REQUIRE(it.value() == 3);

    it = query.seek(2);
    REQUIRE(it.value() == 5);

    it = query.seek(3);
    REQUIRE(it.value() == 7);

    it = query.seek(4);
    REQUIRE( !it );
}

TEST_CASE("seeking in a tree with guaranteed changes", "[query][rank]")
{
    be::mem mem(1024);
    put_result root = make_internal()
        .brn(make_leaf()
           .kv(1, 1).kv(3, 3).put(mem))
        .brn(make_leaf()
           .kv(5, 5).kv(7, 7).put(mem))
        .put(mem);
    query_tree<uint32_t, uint32_t> query(root.nodeID, mem);
    query_tree<uint32_t, uint32_t>::iterator it;

    SECTION("queued insert")
    {
        query.queue_insert(4, 4);

        REQUIRE(query.seek(0).value() == 1);
        REQUIRE(query.seek(1).value() == 3);
        REQUIRE(query.seek(2).value() == 4);
        REQUIRE(query.seek(3).value() == 5);
        REQUIRE(query.seek(4).value() == 7);
    }

    SECTION("queued remove")
    {
        query.queue_remove(3, true);

        REQUIRE(query.seek(0).value() == 1);
        REQUIRE(query.seek(1).value() == 5);
        REQUIRE(query.seek(2).value() == 7);
    }

    SECTION("queued insert then remove")
    {
        query.queue_insert(4, 4);
        query.queue_remove(4, true);

        REQUIRE(query.seek(0).value() == 1);
        REQUIRE(query.seek(1).value() == 3);
        REQUIRE(query.seek(2).value() == 5);
        REQUIRE(query.seek(3).value() == 7);
    }

    SECTION("queued remove then insert")
    {
        query.queue_remove(3, true);
        query.queue_insert(3, 3);

        REQUIRE(query.seek(0).value() == 1);
        REQUIRE(query.seek(1).value() == 3);
        REQUIRE(query.seek(2).value() == 5);
        REQUIRE(query.seek(3).value() == 7);
    }
}

TEST_CASE("seeking in a tree with nonguaranteed changes", "[query][rank]")
{
    be::mem mem(1024);
    put_result root = make_internal()
        .brn(make_leaf()
           .kv(1, 1).kv(3, 3).put(mem))
        .brn(make_leaf()
           .kv(5, 5).kv(7, 7).put(mem))
        .put(mem);
    query_tree<uint32_t, uint32_t> query(root.nodeID, mem);
    query_tree<uint32_t, uint32_t>::iterator it;

    SECTION("queued correct remove")
    {
        query.queue_remove(3, false);

        REQUIRE(query.seek(0).value() == 1);
        REQUIRE(query.seek(1).value() == 5);
        REQUIRE(query.seek(2).value() == 7);
    }

    SECTION("remove the first element of a leaf")
    {
        query.queue_remove(5, false);

        REQUIRE(query.seek(0).value() == 1);
        REQUIRE(query.seek(1).value() == 3);
        REQUIRE(query.seek(2).value() == 7);
    }

    SECTION("queued incorrect remove")
    {
        query.queue_remove(4, false);

        REQUIRE(query.seek(0).value() == 1);
        REQUIRE(query.seek(1).value() == 3);
        REQUIRE(query.seek(2).value() == 5);
        REQUIRE(query.seek(3).value() == 7);
    }

    SECTION("queued insert then remove")
    {
        query.queue_insert(4, 4);
        query.queue_remove(4, false);

        REQUIRE(query.seek(0).value() == 1);
        REQUIRE(query.seek(1).value() == 3);
        REQUIRE(query.seek(2).value() == 5);
        REQUIRE(query.seek(3).value() == 7);
    }

    SECTION("queued incorrect remove then insert")
    {
        query.queue_remove(4, false);
        query.queue_insert(4, 4);

        REQUIRE(query.seek(0).value() == 1);
        REQUIRE(query.seek(1).value() == 3);
        REQUIRE(query.seek(2).value() == 4);
        REQUIRE(query.seek(3).value() == 5);
        REQUIRE(query.seek(4).value() == 7);
    }
}

TEST_CASE("seek into overflow node", "[query][rank]")
{
    be::mem mem(1024);
    put_result root = make_internal()
        .brn(make_leaf()
           .kv(1, 1)
           .kv(3, 3)
           .overflow(make_overflow()
                .val(4)
                .val(5)
                .next(make_overflow()
                      .val(6)
                      .put(mem))
                .put(mem))
           .put(mem))
        .put(mem);
    query_tree<uint32_t, uint32_t> query(root.nodeID, mem);

    REQUIRE( query.seek(3).value() == 5 );
    REQUIRE( query.seek(4).value() == 6 );
}

TEST_CASE("calculate item rank", "[query][rank]")
{
    be::mem mem(1024);
    put_result root = make_internal()
        .brn(make_leaf()
           .kv(1, 1)
           .kv(3, 3)
           .overflow(make_overflow()
                .val(4)
                .val(5)
                .next(make_overflow()
                      .val(6)
                      .put(mem))
                .put(mem))
           .put(mem))
        .brn(make_leaf()
           .kv(7, 7)
           .kv(8, 8)
           .put(mem))
        .put(mem);
    query_tree<uint32_t, uint32_t> query(root.nodeID, mem);
    query_tree<uint32_t, uint32_t>::iterator it;

    SECTION("find rank simple")
    {
        it = query.find(3);
        REQUIRE( it.rank() == 1 );
        it = query.find(7);
        REQUIRE( it.rank() == 5 );
    }

    SECTION("find rank with queued insert")
    {
        query.queue_insert(2, 2);
        it = query.find(3);
        REQUIRE( it.rank() == 2 );
        it = query.find(7);
        REQUIRE( it.rank() == 6 );
    }

    SECTION("find rank in overflow node")
    {
        query.queue_insert(2, 2);
        it = query.find(3);
        ++it;
        ++it;
        ++it;
        REQUIRE( it.rank() == 5 );
    }

    SECTION("find rank with guaranteed queued remove")
    {
        query.queue_remove(1, true);
        it = query.find(3);
        REQUIRE( it.rank() == 0 );
    }

    SECTION("find rank with correct queued remove")
    {
        query.queue_remove(1, false);
        it = query.find(3);
        REQUIRE( it.rank() == 0 );
    }

    SECTION("find rank with wrong remove")
    {
        query.queue_remove(2, false);
        it = query.find(7);
        REQUIRE( it.rank() == 5 );
    }
}

TEST_CASE("queued upsert")
{
    be::mem mem(1024);

    // GIVEN
    put_result root = make_internal()
        .brn(make_leaf()
           .kv(1, 1)
           .put(mem)) // 0
        .brn(make_leaf()
           .kv(3, 3)
           .put(mem)) // 1
        .put(mem); // 2
    query_tree<int, int> query(root.nodeID, mem);

    SECTION("upsert becomes an update")
    {
        query.queue_upsert(1, 2, false);
        REQUIRE( query.find(1).value() == 2 );

        // Check rank as well
        REQUIRE( query.find(3).rank() == 1 );
    }

    SECTION("upsert becomes an insert")
    {
        query.queue_upsert(2, 2, false);
        REQUIRE( query.find(1).value() == 1 );
        REQUIRE( query.find(2).value() == 2 );

        // Check rank as well
        REQUIRE( query.find(3).rank() == 2 );
    }

}
