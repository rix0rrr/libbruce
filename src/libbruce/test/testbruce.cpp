#define CATCH_CONFIG_MAIN
#include <catch/catch.hpp>

#include <libbruce/bruce.h>

// Defines catch's main()

using namespace libbruce;

typedef bruce<int, int> intbruce;

TEST_CASE("abort commit of empty tree should leave blockstore empty")
{
    be::mem mem(1024);
    intbruce b(mem);

    intbruce::edit_ptr t = b.create();
    t->insert(1, 2);
    t->insert(2, 3);
    mutation mut = t->write();

    b.finish(mut, false);

    REQUIRE(mem.blockCount() == 0);
}

TEST_CASE("commit and abort")
{
    be::mem mem(1024);
    intbruce b(mem);

    intbruce::edit_ptr t = b.create();
    t->insert(1, 2);
    t->insert(2, 3);
    mutation mut = t->write();

    REQUIRE(mem.blockCount() == 1);

    SECTION("success commit should leave only new nodes")
    {
        intbruce::edit_ptr u = b.edit(*mut.newRootID());
        u->insert(3, 4);
        mutation mut2 = u->write();

        b.finish(mut2, true);

        REQUIRE_THROWS(mem.get(*mut.newRootID()).size());
    }

    SECTION("abort commit should leave only old nodes")
    {
        intbruce::edit_ptr u = b.edit(*mut.newRootID());
        u->insert(3, 4);
        mutation mut2 = u->write();

        b.finish(mut2, false);

        REQUIRE(mem.blockCount() == 1);
    }
}
