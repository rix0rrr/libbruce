#include <catch/catch.hpp>
#include <libbruce/bruce.h>
#include "serializing.h"

#include <stdio.h>

/**
 * Return the size of a 32-bit int
 */
uint32_t intSize(const void *) { return sizeof(uint32_t); }

int rngcmp(const bruce::range &a, const bruce::range &b)
{
    int ret = memcmp(a.ptr(), b.ptr(), std::min(a.size(), b.size()));
    if (ret != 0) return ret;
    return a.size() - b.size();
}

TEST_CASE("serializing a leaf node is symmetric", "[serializing]" ) {
    // Making a map of ints to ints
    uint32_t one = 1;
    uint32_t two = 2;
    bruce::range one_r(&one, sizeof(one));
    bruce::range two_r(&two, sizeof(two));

    bruce::LeafNodeWriter w;
    w.writePair(one_r, two_r);
    w.writePair(two_r, one_r);

    bruce::range serialized = w.get();
    bruce::leafnode_ptr r = boost::dynamic_pointer_cast<bruce::LeafNode>(ParseNode(serialized, &intSize, &intSize));
    REQUIRE(r->isLeafNode());
    REQUIRE(r->count() == 2);

    REQUIRE( rngcmp(r->key(0), one_r) == 0 );
    REQUIRE( rngcmp(r->value(0), two_r) == 0 );

    REQUIRE( rngcmp(r->key(1), two_r) == 0 );
    REQUIRE( rngcmp(r->value(1), one_r) == 0 );
}

TEST_CASE("serializing an internal node is symmetric", "[serializing]" ) {
    // Making a map of ints to ints
    uint32_t one = 1;
    uint32_t two = 2;
    uint32_t three = 3;
    bruce::range one_r(&one, sizeof(one));
    bruce::range two_r(&two, sizeof(two));
    bruce::range three_r(&three, sizeof(three));

    bruce::InternalNodeWriter w;
    w.writeNode(one_r, 1, 1);
    w.writeNode(two_r, 2, 2);
    w.writeNode(three_r, 3, 3);

    bruce::range serialized = w.get();
    bruce::internalnode_ptr r = boost::dynamic_pointer_cast<bruce::InternalNode>(ParseNode(serialized, &intSize, &intSize));
    REQUIRE(!r->isLeafNode());
    REQUIRE(r->count() == 3);

    /**/
    REQUIRE( r->key(0).empty() );  // Exception
    REQUIRE( r->id(0) == 1 );
    REQUIRE( r->itemCount(0) == 1 );

    /**/
    REQUIRE( rngcmp(r->key(1), two_r) == 0 );
    REQUIRE( r->id(1) == 2 );
    REQUIRE( r->itemCount(1) == 2 );

    /**/
    REQUIRE( rngcmp(r->key(2), three_r) == 0 );
    REQUIRE( r->id(2) == 3 );
    REQUIRE( r->itemCount(2) == 3 );

}
