#include <libbruce/bruce.h>
#include <libbruce/util/blockcache.h>

namespace libbruce { namespace util {

BlockCache::BlockCache(size_t maxSize)
    : m_maxSize(maxSize), m_size(0)
{
}

bool BlockCache::get(nodeid_t id, memory *mem)
{
    map_t::iterator it = m_cache.get<0>().find(id);
    if (it == m_cache.get<0>().end())
        return false;

    *mem = it->block;

    // Remove and re-insert the block to force it to the end of the sequence,
    // effectively updating its lifetime.
    CacheEntry e = *it;
    it = m_cache.get<0>().erase(it);
    m_cache.get<0>().insert(it, e);

    return true;
}

void BlockCache::put(nodeid_t id, const memory &mem)
{
    m_cache.insert(CacheEntry(id, mem));
    m_size += mem.size();

    while (m_size > m_maxSize && !m_cache.empty())
    {
        m_size -= m_cache.get<1>().front().block.size();
        m_cache.get<1>().pop_front();
    }
}

void BlockCache::del(nodeid_t id)
{
    map_t::iterator it = m_cache.get<0>().find(id);
    if (it != m_cache.get<0>().end())
        m_cache.get<0>().erase(it);
}

}};
