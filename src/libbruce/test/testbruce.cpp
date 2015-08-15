#define CATCH_CONFIG_MAIN
#include <catch/catch.hpp>
#include <libbruce/bruce.h>

TEST_CASE( "Test is run", "[factorial]" ) {
    bruce::be::mem mem;
    bruce::bruce b(mem);
    bruce::tree<std::string, uint32_t> t = b.create<std::string, uint32_t>();
    bruce::mutation result = t.insert("hello", 23);
}
