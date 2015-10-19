#include <libbruce/be/disk.h>
#include <cstdio>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

void sha1_compress(uint32_t state[5], const uint8_t block[64]) asm ("sha1_compress");

void sha1_hash(const uint8_t *message, uint32_t len, uint32_t hash[5])
{
	hash[0] = UINT32_C(0x67452301);
	hash[1] = UINT32_C(0xEFCDAB89);
	hash[2] = UINT32_C(0x98BADCFE);
	hash[3] = UINT32_C(0x10325476);
	hash[4] = UINT32_C(0xC3D2E1F0);

	uint32_t i;
	for (i = 0; len - i >= 64; i += 64)
		sha1_compress(hash, message + i);

	uint8_t block[64];
	uint32_t rem = len - i;
	memcpy(block, message + i, rem);

	block[rem] = 0x80;
	rem++;
	if (64 - rem >= 8)
		memset(block + rem, 0, 56 - rem);
	else {
		memset(block + rem, 0, 64 - rem);
		sha1_compress(hash, block);
		memset(block, 0, 56);
	}

	uint64_t longLen = ((uint64_t)len) << 3;
	for (i = 0; i < 8; i++)
		block[64 - 1 - i] = (uint8_t)(longLen >> (i * 8));
	sha1_compress(hash, block);
}

namespace libbruce { namespace be {

disk::disk(std::string pathPrefix, uint32_t maxBlockSize)
    : m_pathPrefix(pathPrefix), m_maxBlockSize(maxBlockSize)
{
}

mempage disk::get(const nodeid_t &id)
{
    int f = open((m_pathPrefix + boost::lexical_cast<std::string>(id)).c_str(), O_RDONLY);
    if (f == -1) throw std::runtime_error("Error opening file for reading");

    struct stat stat_info;
    if (fstat(f, &stat_info) != 0)
        throw std::runtime_error("Error stat'ing file");

    mempage ret(stat_info.st_size);
    read(f, ret.ptr(), stat_info.st_size); // FIXME: Check return code
    close(f);
    return ret;
}

nodeid_t disk::id(const mempage &block)
{
    uint32_t hash[5];
    sha1_hash(block.ptr(), block.size(), hash);

    BOOST_STATIC_ASSERT(sizeof(nodeid_t) == sizeof(hash));

    nodeid_t ret;
    memcpy(ret.data(), hash, sizeof(hash));
    return ret;
}

void disk::put_all(putblocklist_t &blocklist)
{
    if (!m_maxBlockSize)
        throw be_error("Can't put; engine is read-only");

    for (int i = 0; i < blocklist.size(); i++)
    {
        put_one(blocklist[i]);
    }
}

void disk::put_one(putblock_t &block)
{
    int f = open((m_pathPrefix + boost::lexical_cast<std::string>(block.id)).c_str(), O_WRONLY|O_CREAT, 0644);
    // Used to use fstream here but it's too slow
    if (f == -1) throw std::runtime_error(strerror(errno));
    write(f, block.mem.ptr(), block.mem.size()); // FIXME: Check return code
    close(f);

    block.success = true;
}

void disk::del_all(delblocklist_t &ids)
{
    if (!m_maxBlockSize)
        throw be_error("Can't delete; engine is read-only");

    for (int i = 0; i < ids.size(); i++)
    {
        del_one(ids[i]);
    }
}

void disk::del_one(delblock_t &block)
{
    std::remove((m_pathPrefix + boost::lexical_cast<std::string>(block.id)).c_str());
    block.success = true;
}

uint32_t disk::maxBlockSize()
{
    return m_maxBlockSize;
}

}}
