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
    s3be(const std::shared_ptr<Aws::S3::S3Client> &s3, const std::string &bucket, const std::string &prefix, uint32_t blockSize, uint32_t cacheSize);
    ~s3be();

    virtual void newIdentifiers(int n, std::vector<libbruce::nodeid_t> *out);
    virtual libbruce::nodeid_t id(const libbruce::memory &block);
    virtual libbruce::memory get(const libbruce::nodeid_t &id);
    virtual void put_all(libbruce::be::putblocklist_t &blocklist);
    virtual void del_all(libbruce::be::delblocklist_t &ids);
    virtual uint32_t maxBlockSize();
private:
    std::shared_ptr<Aws::S3::S3Client> m_s3;
    std::string m_bucket;
    std::string m_prefix;
    libbruce::util::BlockCache m_cache;

    uint32_t m_blockSize;

    // FIXME: IF I RELEASE IT LIKE THIS I SHOULD BE SHOT
    uint32_t m_blockCtr;

    Aws::S3::Model::PutObjectOutcomeCallable put_one(libbruce::be::putblock_t &block);
    Aws::S3::Model::DeleteObjectOutcomeCallable del_one(libbruce::be::delblock_t &block);
};

}

#endif
