#pragma once
#ifndef AWSBRUCE_S3BE_H
#define AWSBRUCE_S3BE_H

#include <libbruce/bruce.h>
#include <libbruce/util/blockcache.h>
#include <aws/s3/S3Client.h>


namespace awsbruce {

/**
 * Block engine that stores the blocks in S3
 *
 * The blocks will be zipped to decrease transfer times.
 *
 * However, since we can't predict with 100% certainty what compression ratio
 * will be gained, the block size limit is whatever you specify, uncompressed.
 * Any compression gains are just nice-to-have, not guaranteed.
 */
struct s3be : public libbruce::be::be
{
    s3be(const std::shared_ptr<Aws::S3::S3Client> &s3, const std::string &bucket, const std::string &prefix, uint32_t blockSize, uint32_t editQueueSize, uint32_t cacheSize);
    ~s3be();

    virtual libbruce::nodeid_t id(const libbruce::mempage &block);
    virtual libbruce::mempage get(const libbruce::nodeid_t &id);
    virtual libbruce::be::getblockresult_t get_all(const libbruce::be::blockidlist_t &ids);
    virtual void put_all(libbruce::be::putblocklist_t &blocklist);
    virtual void del_all(libbruce::be::delblocklist_t &ids);
    virtual uint32_t maxBlockSize();
    virtual uint32_t editQueueSize();
private:
    std::shared_ptr<Aws::S3::S3Client> m_s3;
    std::string m_bucket;
    std::string m_prefix;
    libbruce::util::BlockCache m_cache;

    uint32_t m_blockSize;
    uint32_t m_editQueueSize;

    Aws::S3::Model::GetObjectOutcomeCallable get_one(const libbruce::nodeid_t &id);
    Aws::S3::Model::PutObjectOutcomeCallable put_one(libbruce::be::putblock_t &block);
    Aws::S3::Model::DeleteObjectOutcomeCallable del_one(libbruce::be::delblock_t &block);

    libbruce::mempage readGetOutcome(const libbruce::nodeid_t &id, Aws::S3::Model::GetObjectOutcome &outcome);
};

void register_s3_engine();

}

#endif
