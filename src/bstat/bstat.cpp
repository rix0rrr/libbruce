/**
 * Statistics calculator for bruce
 *
 * Walks the entire tree and calculates things like depth, degree, fill grade etc.
 *
 * Right now only works for disk-based block engines (which should be good enough for simulations).
 */
#include <stdio.h>

#include <libbruce/bruce.h>
#include "stats.h"

using namespace libbruce;

typedef bruce<std::string, std::string> stringbruce;

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: bstat PREFIX ROOT\n");
        return 1;
    }

    be::disk be(argv[1], 0); // Block size doesn't matter, we're only reading

    nodeid_t id = boost::lexical_cast<nodeid_t>(std::string(argv[2]));

    StatsCollector collector(1024 * 1024);
    walk(id, collector, be, stringbruce::fns);
    collector.print(std::cout);
}
