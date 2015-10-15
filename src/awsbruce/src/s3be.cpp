#include <awsbruce/s3be.h>

#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/s3/model/DeleteObjectRequest.h>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/copy.hpp>
#include <openssl/sha.h>
#include <sstream>

#include <fstream>

#undef to_string

using namespace libbruce;
using namespace libbruce::be;
using namespace Aws::S3;
using namespace Aws::S3::Model;

namespace io = boost::iostreams;

namespace awsbruce {

s3be::s3be(const std::shared_ptr<S3Client> &s3, const std::string &bucket, const std::string &prefix, uint32_t blockSize, uint32_t cacheSize)
    : m_s3(s3), m_bucket(bucket), m_prefix(prefix), m_blockSize(blockSize),
    m_cache(cacheSize)
{
}

s3be::~s3be()
{
}

nodeid_t s3be::id(const libbruce::memory &block)
{
    nodeid_t ret;
    BOOST_STATIC_ASSERT(sizeof(ret) == SHA_DIGEST_LENGTH);
    SHA1(block.byte_ptr(), block.size(), ret.data());
    return ret;
}

memory s3be::get(const nodeid_t &id)
{
    std::cerr << "GET " << id << std::endl;

    // Look in the cache
    {
        memory ret;
        if (m_cache.get(id, &ret))
            return ret;
    }

    GetObjectRequest request;
    request.SetBucket(m_bucket);
    request.SetKey(m_prefix + boost::lexical_cast<std::string>(id));

    GetObjectOutcome response = m_s3->GetObject(request);
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

    memory ret(ss.str().size());
    memcpy((void*)ret.ptr(), ss.str().data(), ss.str().size());

    // Put in the cache
    m_cache.put(id, ret);
    return ret;
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
    std::cerr << "PUT " << block.id << std::endl;
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
    std::cerr << "DEL " << block.id << std::endl;

    DeleteObjectRequest request;
    request.SetBucket(m_bucket);
    request.SetKey(m_prefix + boost::lexical_cast<std::string>(block.id));

    return m_s3->DeleteObjectCallable(request);
}

uint32_t s3be::maxBlockSize()
{
    return m_blockSize;
}

}
