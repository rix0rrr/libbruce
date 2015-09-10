#include <catch/catch.hpp>
#include <libbruce/bruce.h>

#include "query_tree_impl.h"
#include "testhelpers.h"

#include <stdio.h>

using namespace bruce;

TEST_CASE("reading from a tree")
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

TEST_CASE("reading from a tree with queued operations")
{
}
