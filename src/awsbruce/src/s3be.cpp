#include <awsbruce/s3be.h>

#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/s3/model/DeleteObjectRequest.h>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/copy.hpp>
#include <sstream>


#undef to_string

using namespace libbruce;
using namespace libbruce::be;
using namespace Aws::S3;
using namespace Aws::S3::Model;

namespace io = boost::iostreams;

typedef io::filtering_stream<boost::iostreams::bidirectional> filtering_iostream;

namespace awsbruce {

s3be::s3be(const std::shared_ptr<S3Client> &s3, const std::string &bucket, const std::string &prefix, uint32_t blockSize)
    : m_s3(s3), m_bucket(bucket), m_prefix(prefix), m_blockSize(blockSize),
    m_blockCtr(0)
{
}

s3be::~s3be()
{
}

void s3be::newIdentifiers(int n, std::vector<nodeid_t> *out)
{
    for (int i = 0; i < n; i++)
    {
        // FIXME: AAAaaaaahghggh
        out->push_back(++m_blockCtr);
    }
}

memory s3be::get(const nodeid_t &id)
{
    GetObjectRequest request;
    request.SetBucket(m_bucket);
    request.SetKey(m_prefix + std::to_string(id));

    GetObjectOutcome response = m_s3->GetObject(request);
    if (!response.IsSuccess())
        throw be_error(response.GetError().GetMessage().c_str());

    std::stringstream ss;

    // Decompress into the stringstream
    io::filtering_ostreambuf in;
    in.push(io::zlib_decompressor());
    in.push(ss);
    io::copy(response.GetResult().GetBody(), in);

    memory ret(ss.str().size());
    memcpy((void*)ret.ptr(), ss.str().data(), ss.str().size());
    return ret;
}

void s3be::put_all(putblocklist_t &blocklist)
{
    std::vector<PutObjectOutcomeCallable> ops;
    ops.reserve(blocklist.size());

    for (int i = 0; i < blocklist.size(); i++)
        ops.push_back(put_one(blocklist[i]));

    // Collect results
    for (int i = 0; i < blocklist.size(); i++)
    {
        PutObjectOutcome response = ops[i].get();
        blocklist[i].success = response.IsSuccess();
        if (!response.IsSuccess()) blocklist[i].failureReason = response.GetError().GetMessage();
    }
}

PutObjectOutcomeCallable s3be::put_one(libbruce::be::putblock_t &block)
{
    io::basic_array_source<char> memstream((char*)block.mem.ptr(), block.mem.size());

    std::shared_ptr<std::stringstream> ss = std::make_shared<std::stringstream>();

    io::filtering_ostream in;
    in.push(io::zlib_compressor());
    in.push(*ss);
    io::copy(memstream, in);

    PutObjectRequest request;
    request.SetBucket(m_bucket);
    request.SetKey(m_prefix + std::to_string(block.id));
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
        if (!response.IsSuccess()) ids[i].failureReason = response.GetError().GetMessage();
    }
}

DeleteObjectOutcomeCallable s3be::del_one(libbruce::be::delblock_t &block)
{
    DeleteObjectRequest request;
    request.SetBucket(m_bucket);
    request.SetKey(m_prefix + std::to_string(block.id));

    return m_s3->DeleteObjectCallable(request);
}

uint32_t s3be::maxBlockSize()
{
    return m_blockSize;
}

}
