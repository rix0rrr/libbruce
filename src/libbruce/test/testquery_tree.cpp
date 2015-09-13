#include <catch/catch.hpp>
#include <libbruce/bruce.h>

#include "query_tree_impl.h"
#include "testhelpers.h"

#include <stdio.h>

using namespace bruce;

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

TEST_CASE("reading from a tree with queued delete", "[query]")
{
    be::mem mem(1024);
    edit_tree<int, int> t(maybe_nodeid(), mem);
    t.insert(1, 1);
    mutation mut = t.flush();

    query_tree<int, int> q(*mut.newRootID(), mem);
    q.queue_remove(1, true);
    REQUIRE( !q.get(1) );
}

TEST_CASE("reading from a tree with queued delete and insert", "[query]")
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

TEST_CASE("reading from a tree with queued insert and delete", "[query]")
{
    be::mem mem(1024);
    edit_tree<int, int> t(maybe_nodeid(), mem);
    t.insert(0, 0);
    mutation mut = t.flush();

    query_tree<int, int> q(*mut.newRootID(), mem);
    q.queue_insert(1, 1);
    q.queue_remove(1, true);
    REQUIRE( !*q.get(1) );
}
