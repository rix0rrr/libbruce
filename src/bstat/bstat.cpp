/**
 * Statistics calculator for bruce
 *
 * Walks the entire tree and calculates things like depth, degree, fill grade etc.
 *
 * Right now only works for disk-based block engines (which should be good enough for simulations).
 */
#include <stdio.h>

#include <libbruce/bruce.h>
#include <libbruce/util/be_registry.h>
#include "stats.h"

#ifdef HAVE_AWSBRUCE
#include <awsbruce/awsbruce.h>
#endif

using namespace libbruce;

typedef bruce<std::string, std::string> stringbruce;

int main(int argc, char* argv[])
{
    be::register_disk_engine();
    be::register_mem_engine();
#ifdef HAVE_AWSBRUCE
    awsbruce::register_s3_engine();
#endif

    const char *be_spec = std::getenv("BRUCE_BE");
    if (!be_spec)
    {
        std::cerr << "Set BRUCE_BE variable to block engine spec" << std::endl;
        return 1;
    }

    if (argc != 2)
    {
        fprintf(stderr, "Usage: bstat ROOT\n");
        return 1;
    }

    nodeid_t id = boost::lexical_cast<nodeid_t>(std::string(argv[1]));

    be::be_ptr be = util::create_be(std::string(be_spec));

    StatsCollector collector(be->maxBlockSize(), be->editQueueSize());
    walk(id, collector, *be, stringbruce::fns);
    collector.print(std::cout);
}
