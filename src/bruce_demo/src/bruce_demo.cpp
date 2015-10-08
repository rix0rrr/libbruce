#include <libbruce/bruce.h>
#include <awsbruce/awsbruce.h>

#include <boost/iostreams/stream.hpp>
#include <aws/core/utils/logging/AWSLogging.h>
#include <aws/core/utils/logging/DefaultLogSystem.h>
#include <aws/core/utils/logging/LogLevel.h>
#include <aws/core/auth/AWSCredentialsProviderChain.h>
#include <aws/core/http/HttpClientFactory.h>

using namespace libbruce;
using namespace awsbruce;

using namespace Aws::Auth;
using namespace Aws::Http;
using namespace Aws::Client;
using namespace Aws::S3;
using namespace Aws::Utils;

namespace io = boost::iostreams;

const int MB = 1024 * 1024;

#define S3_BUCKET "huijbers-test"
#define S3_PREFIX "bruce/"

void noop(void *p)
{
}

int main(int argc, char* argv[])
{
    std::shared_ptr<Aws::OStream> output(&std::cerr, noop);
    auto logger = std::make_shared<Aws::Utils::Logging::DefaultLogSystem>(Aws::Utils::Logging::LogLevel::Warn, output);
    Aws::Utils::Logging::InitializeAWSLogging(logger);

    // Create a client
    ClientConfiguration config;
    config.region = Aws::Region::EU_WEST_1;
    config.scheme = Scheme::HTTPS;
    config.connectTimeoutMs = 30000;
    config.requestTimeoutMs = 30000;

    auto clientFactory = Aws::MakeShared<HttpClientFactory>(NULL);
    auto s3 = Aws::MakeShared<S3Client>(NULL, Aws::MakeShared<DefaultAWSCredentialsProviderChain>(NULL), config, clientFactory);

    s3be be(s3, S3_BUCKET, S3_PREFIX, 1 * MB);
    bruce b(be);
    auto edit = b.create<std::string, std::string>();
    edit->insert("key", "value");
    mutation mut = edit->flush();
    b.finish(mut, true);

    if (mut.success())
    {
        auto query = b.query<std::string, std::string>(*mut.newRootID());

        for (query_tree<std::string, std::string>::iterator it = query->begin(); it != query->end(); ++it)
        {
            std::cout << it.key() << " -> " << it.value() << std::endl;
        }
    }

    Aws::Utils::Logging::ShutdownAWSLogging();
}
