#define CATCH_CONFIG_MAIN
#include <catch/catch.hpp>

#include <libbruce/bruce.h>

// Defines catch's main()

using namespace libbruce;

TEST_CASE("abort commit of empty tree should leave blockstore empty")
{
    be::mem mem(1024);
    ::bruce::bruce b(mem);

    edit_tree<int, int>::ptr t = b.create<int, int>();
    t->insert(1, 2);
    t->insert(2, 3);
    mutation mut = t->flush();

    b.finish(mut, false);

    REQUIRE(mem.blockCount() == 0);
}

TEST_CASE("commit and abort")
{
    be::mem mem(1024);
    ::bruce::bruce b(mem);

    edit_tree<int, int>::ptr t = b.create<int, int>();
    t->insert(1, 2);
    t->insert(2, 3);
    mutation mut = t->flush();

    REQUIRE(mem.blockCount() == 1);

    SECTION("success commit should leave only new nodes")
    {
        edit_tree<int, int>::ptr u = b.edit<int, int>(*mut.newRootID());
        u->insert(3, 4);
        mutation mut2 = u->flush();

        b.finish(mut2, true);

        REQUIRE_THROWS(mem.get(*mut.newRootID()).size());
    }

    SECTION("abort commit should leave only old nodes")
    {
        edit_tree<int, int>::ptr u = b.edit<int, int>(*mut.newRootID());
        u->insert(3, 4);
        mutation mut2 = u->flush();

        b.finish(mut2, false);

        REQUIRE(mem.blockCount() == 1);
    }
}
