#include <awsbruce/s3be.h>
#include <libbruce/util/be_registry.h>

#include <aws/core/auth/AWSCredentialsProviderChain.h>
#include <aws/core/http/HttpClientFactory.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/s3/model/DeleteObjectRequest.h>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/copy.hpp>
#include <openssl/sha.h>
#include <sstream>

#undef to_string

using namespace libbruce;
using namespace libbruce::be;
using namespace Aws::Auth;
using namespace Aws::Http;
using namespace Aws::Client;
using namespace Aws::S3;
using namespace Aws::S3::Model;

namespace io = boost::iostreams;

namespace awsbruce {

s3be::s3be(const std::shared_ptr<S3Client> &s3, const std::string &bucket, const std::string &prefix, uint32_t blockSize, uint32_t editQueueSize, uint32_t cacheSize)
    : m_s3(s3), m_bucket(bucket), m_prefix(prefix), m_blockSize(blockSize), m_editQueueSize(editQueueSize), m_cache(cacheSize)
{
}

s3be::~s3be()
{
}

nodeid_t s3be::id(const libbruce::mempage &block)
{
    nodeid_t ret;
    BOOST_STATIC_ASSERT(sizeof(ret) == SHA_DIGEST_LENGTH);
    SHA1(block.ptr(), block.size(), ret.data());
    return ret;
}

mempage s3be::get(const nodeid_t &id)
{
    //std::cerr << "GET " << id << std::endl;

    // Look in the cache
    {
        mempage ret;
        if (m_cache.get(id, &ret))
            return ret;
    }

    GetObjectRequest request;
    request.SetBucket(m_bucket);
    request.SetKey(m_prefix + boost::lexical_cast<std::string>(id));

    GetObjectOutcome response = m_s3->GetObject(request);
    return readGetOutcome(id, response);
}

getblockresult_t s3be::get_all(const blockidlist_t &ids)
{
    getblockresult_t ret;

    std::vector<nodeid_t> queried_ids;
    std::vector<GetObjectOutcomeCallable> ops;
    queried_ids.reserve(ids.size());
    ops.reserve(ids.size());

    // Look in the cache, query only if not found in the cache
    for (int i = 0; i < ids.size(); i++)
    {
        mempage cached;
        if (m_cache.get(ids[i], &cached))
            ret[ids[i]] = cached;
        else
        {
            queried_ids.push_back(ids[i]);
            ops.push_back(get_one(ids[i]));
        }

    }

    // Collect results
    for (int i = 0; i < queried_ids.size(); i++)
    {
        GetObjectOutcome response = ops[i].get();
        ret[queried_ids[i]] = readGetOutcome(queried_ids[i], response);
    }

    return ret;
}

mempage s3be::readGetOutcome(const nodeid_t &id, GetObjectOutcome &response)
{
    if (!response.IsSuccess())
        throw be_error((std::string("Error fetching ") +
                       boost::lexical_cast<std::string>(id) +
                       ": " +
                       response.GetError().GetMessage()).c_str());

    std::stringstream ss;

    // Decompress into the stringstream
    io::filtering_ostreambuf in;
    in.push(io::zlib_decompressor());
    in.push(ss);
    io::copy(response.GetResult().GetBody(), in);

    mempage ret(ss.str().size());
    memcpy((void*)ret.ptr(), ss.str().data(), ss.str().size());

    // Put in the cache
    m_cache.put(id, ret);
    return ret;
}

Aws::S3::Model::GetObjectOutcomeCallable s3be::get_one(const libbruce::nodeid_t &id)
{
    GetObjectRequest request;
    request.SetBucket(m_bucket);
    request.SetKey(m_prefix + boost::lexical_cast<std::string>(id));

    return m_s3->GetObjectCallable(request);
}

void s3be::put_all(putblocklist_t &blocklist)
{
    std::vector<PutObjectOutcomeCallable> ops;
    ops.reserve(blocklist.size());

    for (int i = 0; i < blocklist.size(); i++)
    {
        ops.push_back(put_one(blocklist[i]));
    }

    // Collect results
    for (int i = 0; i < blocklist.size(); i++)
    {
        PutObjectOutcome response = ops[i].get();
        blocklist[i].success = response.IsSuccess();

        // Only cache succesful puts
        if (response.IsSuccess())
            m_cache.put(blocklist[i].id, blocklist[i].mem);
        else
            blocklist[i].failureReason = response.GetError().GetMessage();
    }
}

PutObjectOutcomeCallable s3be::put_one(libbruce::be::putblock_t &block)
{
    //std::cerr << "PUT " << block.id << std::endl;
    io::basic_array_source<char> memstream((char*)block.mem.ptr(), block.mem.size());

    std::shared_ptr<std::stringstream> ss = std::make_shared<std::stringstream>();

    io::filtering_ostream in;
    in.push(io::zlib_compressor());
    in.push(*ss);
    io::copy(memstream, in);

    PutObjectRequest request;
    request.SetBucket(m_bucket);
    request.SetKey(m_prefix + boost::lexical_cast<std::string>(block.id));
    request.SetContentType("application/octet-stream");
    request.SetBody(ss);
    request.SetContentLength(ss->str().size());

    return m_s3->PutObjectCallable(request);
}

void s3be::del_all(delblocklist_t &ids)
{
    std::vector<DeleteObjectOutcomeCallable> ops;
    ops.reserve(ids.size());

    // Start dels
    for (int i = 0; i < ids.size(); i++)
    {
        ops.push_back(del_one(ids[i]));
    }

    // Collect results
    for (int i = 0; i < ids.size(); i++)
    {
        DeleteObjectOutcome response = ops[i].get();
        ids[i].success = response.IsSuccess();
        if (response.IsSuccess())
            m_cache.del(ids[i].id);
        else
            ids[i].failureReason = response.GetError().GetMessage();
    }
}

DeleteObjectOutcomeCallable s3be::del_one(libbruce::be::delblock_t &block)
{
    //std::cerr << "DEL " << block.id << std::endl;

    DeleteObjectRequest request;
    request.SetBucket(m_bucket);
    request.SetKey(m_prefix + boost::lexical_cast<std::string>(block.id));

    return m_s3->DeleteObjectCallable(request);
}

uint32_t s3be::maxBlockSize()
{
    return m_blockSize;
}

uint32_t s3be::editQueueSize()
{
    return m_editQueueSize;
}

be_ptr create_s3_engine(const std::string &location, size_t block_size, size_t queue_size, const util::options_t &options)
{
    ClientConfiguration config;
    config.region = Aws::Region::EU_WEST_1;
    config.scheme = Scheme::HTTPS;
    config.connectTimeoutMs = options.get("timeout", 10000);
    config.requestTimeoutMs = options.get("timeout", 10000);

    auto clientFactory = Aws::MakeShared<HttpClientFactory>(NULL);
    auto s3 = Aws::MakeShared<S3Client>(NULL, Aws::MakeShared<DefaultAWSCredentialsProviderChain>(NULL), config, clientFactory);

    int slash = location.find("/");
    std::string bucket = location.substr(0, slash);
    std::string prefix = slash == std::string::npos ? "" : location.substr(slash + 1);
    size_t cache_size = options.get("cache", 100 * 1024 * 1024);

    return boost::make_shared<s3be>(s3, bucket, prefix, block_size, queue_size, cache_size);
}

void register_s3_engine()
{
    util::register_be_factory("s3", &create_s3_engine);
}

}
