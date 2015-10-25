/**
 * Testing the typesafe interface
 */
#include <catch/catch.hpp>
#include "testhelpers.h"
#include "leaf_node.h"
#include <libbruce/bruce.h>

#include <stdio.h>

using namespace libbruce;

kv_pair mkPair(int key, int value)
{
    return kv_pair(intCopy(key), intCopy(value));
}

struct ApplyTests
{
    ApplyTests()
    {
        leaf = boost::make_shared<LeafNode>(intToIntTree);
        leaf->insert(mkPair(5, 5));
        leaf->insert(mkPair(10, 10));
    }

    void insert(int key, int value)
    {
        edits.push_back(pending_edit(INSERT, intCopy(key), intCopy(value), true));
    }

    void upsert(int key, int value)
    {
        edits.push_back(pending_edit(UPSERT, intCopy(key), intCopy(value), false));
    }

    void remove(int key)
    {
        edits.push_back(pending_edit(REMOVE_KEY, intCopy(key), memslice(), false));
    }

    void remove(int key, int value)
    {
        edits.push_back(pending_edit(REMOVE_KV, intCopy(key), intCopy(value), false));
    }

    void applyAll()
    {
        leaf->applyAll(edits.begin(), edits.end());

        for (pairlist_t::iterator it = leaf->pairs.begin(); it != leaf->pairs.end(); ++it)
        {
            if (it != leaf->pairs.begin()) values += ", ";
            int *p = (int*)it->second.ptr();
            if (p) values += boost::lexical_cast<std::string>(*p);
        }
    }

    void verifySize()
    {
        // Construct a new leaf node and check that the size of the modified leaf node is the same.
        leafnode_ptr reference = boost::make_shared<LeafNode>(leaf->pairs.begin(), leaf->pairs.end(), intToIntTree);
        REQUIRE( reference->elementsSize() == leaf->elementsSize() );
    }

    leafnode_ptr leaf;
    editlist_t edits;
    std::string values;
};

//----------------------------------------------------------------------

TEST_CASE_METHOD(ApplyTests, "multiple inserts")
{
    insert(1, 1);
    insert(7, 7);
    insert(12, 12);

    applyAll();

    REQUIRE( values == "1, 5, 7, 10, 12" );
    verifySize();
}

TEST_CASE_METHOD(ApplyTests, "insert then remove by key")
{
    SECTION("in the middle")
    {
        insert(7, 7);
        remove(7);
        applyAll();

        REQUIRE( values == "5, 10" );
        verifySize();
    }

    SECTION("at the end")
    {
        insert(12, 12);
        remove(12);
        applyAll();

        REQUIRE( values == "5, 10" );
        verifySize();
    }
}

TEST_CASE_METHOD(ApplyTests, "remove then insert")
{
    SECTION("in the middle")
    {
        remove(7);
        insert(7, 7);
        applyAll();

        REQUIRE( values == "5, 7, 10" );
        verifySize();
    }

    SECTION("at the end")
    {
        remove(12);
        insert(12, 12);
        applyAll();

        REQUIRE( values == "5, 10, 12" );
        verifySize();
    }
}

TEST_CASE_METHOD(ApplyTests, "insert, insert then delete 1st by value")
{
    SECTION("in the middle")
    {
        insert(7, 7);
        insert(7, 8);
        remove(7, 7);
        applyAll();

        REQUIRE( values == "5, 8, 10" );
        verifySize();
    }

    SECTION("at the end")
    {
        insert(12, 12);
        insert(12, 13);
        remove(12, 12);
        applyAll();

        REQUIRE( values == "5, 10, 13" );
        verifySize();
    }
}

TEST_CASE_METHOD(ApplyTests, "remove then upsert")
{
    SECTION("existing")
    {
        remove(5);
        upsert(5, 6);
        applyAll();

        REQUIRE( values == "6, 10" );
        verifySize();
    }

    SECTION("in the middle")
    {
        remove(7);
        upsert(7, 7);
        applyAll();

        REQUIRE( values == "5, 7, 10" );
        verifySize();
    }

    SECTION("at the end")
    {
        remove(12);
        upsert(12, 12);
        applyAll();

        REQUIRE( values == "5, 10, 12" );
        verifySize();
    }
}

TEST_CASE_METHOD(ApplyTests, "upsert then remove")
{
    SECTION("existing")
    {
        upsert(5, 6);
        remove(5);
        applyAll();

        REQUIRE( values == "10" );
        verifySize();
    }

    SECTION("in the middle")
    {
        upsert(7, 7);
        remove(7);
        applyAll();

        REQUIRE( values == "5, 10" );
        verifySize();
    }

    SECTION("at the end")
    {
        upsert(12, 12);
        remove(12);
        applyAll();

        REQUIRE( values == "5, 10" );
        verifySize();
    }
}
