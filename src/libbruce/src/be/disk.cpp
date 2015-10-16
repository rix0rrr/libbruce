#include <libbruce/be/disk.h>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/copy.hpp>
#include <cstdio>

#include <sha1.h>
#include <fstream>

namespace io = boost::iostreams;

namespace libbruce { namespace be {

disk::disk(std::string pathPrefix, uint32_t maxBlockSize)
    : m_pathPrefix(pathPrefix), m_maxBlockSize(maxBlockSize)
{
}

memory disk::get(const nodeid_t &id)
{
    std::fstream file(m_pathPrefix + boost::lexical_cast<std::string>(id),
                      std::fstream::in | std::fstream::binary | std::fstream::ate);
    size_t size = file.tellg();
    file.seekg(0);

    memory ret(size);
    io::basic_array_sink<char> memstream((char*)ret.ptr(), ret.size());
    io::copy(file, memstream);

    return ret;
}

typedef io::stream<io::array_source > memsourcestream;

nodeid_t disk::id(const memory &block)
{
    io::stream<io::array_source> memstream(io::array_source((char*)block.ptr(), block.size()));

    SHA1 digest;
    digest.update(memstream);
    std::string hash = digest.final();

    nodeid_t ret = boost::lexical_cast<nodeid_t>(hash);
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
    io::basic_array_source<char> memstream((char*)block.mem.ptr(), block.mem.size());

    std::fstream file(m_pathPrefix + boost::lexical_cast<std::string>(block.id),
                      std::fstream::out | std::fstream::binary | std::fstream::trunc);
    io::copy(memstream, file);
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
