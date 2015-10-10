#include <catch/catch.hpp>

#include <libbruce/bruce.h>

using namespace libbruce;


TEST_CASE("Deserialize node ID")
{
    nodeid_t id = boost::lexical_cast<nodeid_t>("0102030405060708090a0b0c0d0e0f1011121314");

    for (int i = 0; i < 20; i++)
        REQUIRE(id.data()[i] == i + 1);
}
