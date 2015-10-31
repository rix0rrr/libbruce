#include <libbruce/bruce.h>
#include <libbruce/util/be_registry.h>

#include <sys/time.h>
#include <fstream>
#include <cstdlib>

using namespace libbruce;

#ifdef HAVE_AWSBRUCE
#include <awsbruce/awsbruce.h>

#include <aws/core/utils/logging/AWSLogging.h>
#include <aws/core/utils/logging/DefaultLogSystem.h>
#include <aws/core/utils/logging/LogLevel.h>
#include <aws/core/auth/AWSCredentialsProviderChain.h>
#include <aws/core/http/HttpClientFactory.h>

using namespace awsbruce;

using namespace Aws::Auth;
using namespace Aws::Http;
using namespace Aws::Client;
using namespace Aws::S3;
using namespace Aws::Utils;
#endif

typedef bruce<std::string, std::string> stringbruce;

struct Timer
{
    Timer()
    {
        timeval now;
        gettimeofday(&now, NULL);

        m_start = now.tv_sec + (now.tv_usec / 1e6);
    }

    /**
     * Return elapsed time in ms
     */
    size_t elapsed()
    {
        timeval now;
        gettimeofday(&now, NULL);

        double t = now.tv_sec + (now.tv_usec / 1e6);
        return (t - m_start) * 1000;
    }

private:
    double m_start;
};

void noop(void *p)
{
}

struct Params
{
    std::string be_spec;
    maybe_nodeid root;
    bool hasKey = false;
    std::string key;
    bool hasValue = false;
    std::string value;
    std::map<std::string, std::string> otherKVs;
    std::fstream metrics;
};

bool parseParams(int argc, char* argv[], Params &ret)
{
    const char *be_spec = std::getenv("BRUCE_BE");
    if (!be_spec)
    {
        std::cerr << "Set BRUCE_BE variable to block engine spec" << std::endl;
        return false;
    }
    ret.be_spec = std::string(be_spec);

    if (argc == 1)
    {
        std::cout << "Usage:" << std::endl;
        std::cout << "    bkv '' KEY VALUE" << std::endl;
        std::cout << "    bkv ID" << std::endl;
        std::cout << "    bkv ID KEY" << std::endl;
        std::cout << "    bkv ID KEY VALUE [KEY VALUE [...]]" << std::endl;
        return false;
    }

    ret.metrics.open("metrics.csv", std::fstream::out | std::fstream::app);

    // Parse params
    if (strlen(argv[1]))
        ret.root = boost::lexical_cast<nodeid_t>(argv[1]);

    if (argc >= 3)
    {
        ret.hasKey = true;
        ret.key = std::string(argv[2]);
    }

    if (argc >= 4)
    {
        ret.hasValue = true;
        ret.value = std::string(argv[3]);
    }

    for (int i = 4; i + 1 < argc; i += 2)
    {
        ret.otherKVs[std::string(argv[i])] = std::string(argv[i + 1]);
    }

    return true;
}

int putKeyValue(stringbruce &b, Params &params)
{
    Timer t;
    auto edit = params.root ? b.edit(*params.root) : b.create();
    edit->insert(params.key, params.value);

    for (std::map<std::string, std::string>::const_iterator it = params.otherKVs.begin(); it != params.otherKVs.end(); ++it)
        edit->insert(it->first, it->second);

    mutation mut = edit->write();
#ifndef PERFTEST
    b.finish(mut, true);
    if (!mut.success())
    {
        fprintf(stderr, "Error!\n");
        return 1;
    }
#endif

    params.metrics << "put," << 1 + params.otherKVs.size() << "," << t.elapsed() << std::endl;

    std::cout << *mut.newRootID() << std::endl;
    return 0;
}

int queryKey(stringbruce &b, Params &params)
{
    if (!params.root)
    {
        fprintf(stderr, "Can't query without a root\n");
        return 1;
    }

    auto query = b.query(*params.root);
    stringbruce::iterator it = query->find(params.key);

    while (it && it.key() == params.key)
    {
        std::cout << it.key() << " -> " << it.value() << std::endl;
        ++it;
    }


    return 0;
}

int fullScan(stringbruce &b, Params &params)
{
    auto query = b.query(*params.root);
    stringbruce::iterator it = query->begin();

    while (it)
    {
        std::cout << it.key() << " -> " << it.value() << std::endl;
        ++it;
    }

    return 0;
}

int main(int argc, char* argv[])
{
    be::register_disk_engine();
    be::register_mem_engine();
#ifdef HAVE_AWSBRUCE
    awsbruce::register_s3_engine();

    std::shared_ptr<Aws::OStream> output(&std::cerr, noop);
    auto logger = std::make_shared<Aws::Utils::Logging::DefaultLogSystem>(Aws::Utils::Logging::LogLevel::Warn, output);
    Aws::Utils::Logging::InitializeAWSLogging(logger);

#endif

    try
    {
        // Params
        Params params;
        if (!parseParams(argc, argv, params))
            return 1;

        be::be_ptr be = util::create_be(params.be_spec);
        stringbruce b(*be);

        int exit;
        try
        {
            if (params.hasKey && params.hasValue)
                exit = putKeyValue(b, params);
            else if (params.hasKey)
                exit = queryKey(b, params);
            else
                exit = fullScan(b, params);
        }
        catch (std::runtime_error &e)
        {
            std::cerr << e.what();
            exit = 1;
        }

#ifdef HAVE_AWSBRUCE
        Aws::Utils::Logging::ShutdownAWSLogging();
#endif

        return exit;
    }
    catch (std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}
