/**
 * Testing the typesafe interface
 */
#include <catch/catch.hpp>
#include "testhelpers.h"
#include "serializing.h"
#include <libbruce/bruce.h>

#include <stdio.h>

using namespace libbruce;

TEST_CASE("tree with a flushable edit queue")
{
    be::mem mem(1024, 256);

    // GIVEN
    put_result root = make_internal()
        .brn(make_leaf(intToIntTree)
           .kv(1, 1)
           .put(mem)) // 0
        .brn(make_leaf(intToIntTree)
           .kv(3, 3)
           .put(mem)) // 1
        .put(mem); // 2

    REQUIRE( mem.blockCount() == 3 );

    // WHEN
    SECTION("not enough changes to flush only flushes top block")
    {
        edit_tree<int, int> edit(root.nodeID, mem);
        for (int i = 0; i < 25; i++)
            edit.insert(i, i);

        mutation mut = edit.flush();
        REQUIRE( mem.blockCount() == 4 );
    }

    SECTION("large amount of changes causes a flush to all blocks")
    {
        edit_tree<int, int> edit(root.nodeID, mem);
        for (int i = 0; i < 33; i++)
            edit.insert(i, i);

        mutation mut = edit.flush();
        REQUIRE( mem.blockCount() == 6 );
    }
}

TEST_CASE("flushing edit queue causes a leaf node to split")
{
    be::mem mem(256, 256);

    // GIVEN
    put_result root = make_internal()
        .brn(make_leaf(intToIntTree)
           .kv(1, 1)
           .put(mem)) // 0
        .brn(make_leaf(intToIntTree)
           .kv(40, 40)
           .put(mem)) // 1
        .put(mem); // 2

    edit_tree<int, int> edit(root.nodeID, mem);
    for (int i = 0; i < 33; i++)
        edit.insert(i, i);

    mutation mut = edit.flush();
    REQUIRE( mem.blockCount() == 6 ); // 3 + 1 new root + 2 new leaves

    // Verify that the new root has no more pending changes
    internalnode_ptr internal = loadInternal(mem, *mut.newRootID());
    REQUIRE( internal->editQueue.empty() );
}

TEST_CASE("splitting an internal node does not lose the edit queue")
{
    be::mem mem(65 + 256, 256);

    // GIVEN (a node that must split on flush)
    put_result root = make_internal()
        .brn(make_leaf(intToIntTree)
           .kv(1, 1)
           .put(mem))
        .brn(make_leaf(intToIntTree)
           .kv(5, 5)
           .put(mem))
        .brn(make_leaf(intToIntTree)
           .kv(10, 10)
           .put(mem))
        .edit(pending_edit(INSERT, intCopy(3), intCopy(3), true))
        .edit(pending_edit(REMOVE_KEY, intCopy(5), memslice(), true))
        .put(mem);

    edit_tree<int, int> edit(root.nodeID, mem);
    edit.insert(6, 6); // Add an edit to force the node to update
    mutation mut = edit.flush();

    // Verify that the changes are still in the tree (we don't care
    // about where they are)
    internalnode_ptr internal = loadInternal(mem, *mut.newRootID());
    internalnode_ptr left = loadInternal(mem, internal->branches[0].nodeID);
    internalnode_ptr right = loadInternal(mem, internal->branches[1].nodeID);

    int edits = internal->editQueue.size() + left->editQueue.size() + right->editQueue.size();
    REQUIRE( edits == 3 );
}

TEST_CASE("finding with an edit queue")
{
    be::mem mem(512, 256);

    // GIVEN (a node that must split on flush)
    put_result root = make_internal()
        .brn(make_leaf(intToIntTree)
           .kv(1, 1)
           .put(mem))
        .brn(make_leaf(intToIntTree)
           .kv(5, 5)
           .put(mem))
        .brn(make_leaf(intToIntTree)
           .kv(10, 10)
           .put(mem))
        .edit(pending_edit(INSERT, intCopy(3), intCopy(3), true))
        .edit(pending_edit(REMOVE_KEY, intCopy(5), memslice(), true))
        .put(mem);

    query_tree<int, int> q(root.nodeID, mem);
    query_tree<int, int>::iterator it = q.find(3);

    REQUIRE( it.value() == 3 );
}

TEST_CASE("seeking with an edit queue")
{
    be::mem mem(512, 256);

    // GIVEN (a node that must split on flush)
    put_result root = make_internal()
        .brn(make_leaf(intToIntTree)
           .kv(1, 1)
           .put(mem))
        .brn(make_leaf(intToIntTree)
           .kv(5, 5)
           .put(mem))
        .brn(make_leaf(intToIntTree)
           .kv(10, 10)
           .put(mem))
        .edit(pending_edit(INSERT, intCopy(3), intCopy(3), true))
        .edit(pending_edit(REMOVE_KEY, intCopy(5), memslice(), true))
        .put(mem);

    query_tree<int, int> q(root.nodeID, mem);
    query_tree<int, int>::iterator it = q.seek(2);

    REQUIRE( it.value() == 10 );
}

// FIXME: Seeking in a tree with 2 internal nodes with queued edits.
