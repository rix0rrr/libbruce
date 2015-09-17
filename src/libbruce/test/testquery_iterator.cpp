#include <catch/catch.hpp>
#include "testhelpers.h"
#include "serializing.h"
#include <libbruce/bruce.h>

#include <stdio.h>

using namespace bruce;

TEST_CASE("iteration crossing between leafs")
{
    be::mem mem(1024);

    put_result root = make_internal()
        .brn(make_leaf()
           .kv(1, 1).kv(3, 3).put(mem))
        .brn(make_leaf()
           .kv(5, 5).kv(7, 7).put(mem))
        .put(mem);

    query_tree<uint32_t, uint32_t> query(root.nodeID, mem);

    SECTION("iterate over all")
    {
        query_tree<uint32_t, uint32_t>::iterator it = query.find(1);
        REQUIRE( it );
        REQUIRE( it.value() == 1 );
        ++it;
        REQUIRE( it.value() == 3 );
        ++it;
        REQUIRE( it.value() == 5 );
        ++it;
        REQUIRE( it.value() == 7 );
        ++it;
        REQUIRE( !it );
    }
}

TEST_CASE("iteration across overflow nodes")
{
    be::mem mem(1024);

    put_result root = make_internal()
        .brn(make_leaf()
           .kv(1, 1)
           .kv(3, 3)
           .overflow(make_overflow()
                .val(30)
                .next(make_overflow()
                    .val(31)
                    .put(mem))
                .put(mem))
           .put(mem))
        .brn(make_leaf()
           .kv(5, 5)
           .kv(7, 7)
           .put(mem))
        .put(mem);

    query_tree<uint32_t, uint32_t> query(root.nodeID, mem);

    SECTION("iterate over all")
    {
        query_tree<uint32_t, uint32_t>::iterator it = query.find(1);
        REQUIRE( it );
        REQUIRE( it.value() == 1 );
        ++it;
        REQUIRE( it.value() == 3 );
        ++it;
        REQUIRE( it.value() == 30 );
        ++it;
        REQUIRE( it.value() == 31 );
        ++it;
        REQUIRE( it.value() == 5 );
        ++it;
        REQUIRE( it.value() == 7 );
        ++it;
        REQUIRE( !it );
    }
}

TEST_CASE("iteration ends after overflow")
{
    be::mem mem(1024);

    put_result root = make_leaf()
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
        REQUIRE( it.value() == 1 );
        ++it;
        REQUIRE( it.value() == 3 );
        ++it;
        REQUIRE( it );
        REQUIRE( it.value() == 30 );
        ++it;
        REQUIRE( !it );
    }
}

// Tree two levels deep

// Iterate with queued inserts

// Rank stuff (with guaranteed and unguaranteed queued inserts)
