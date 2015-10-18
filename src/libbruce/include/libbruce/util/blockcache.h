#pragma once
#ifndef LIBBRUCE_BLOCKCCACHE_H
#define LIBBRUCE_BLOCKCCACHE_H

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/member.hpp>

#include <libbruce/bruce.h>

namespace libbruce { namespace util {

struct CacheEntry
{
    CacheEntry(nodeid_t id, const mempage &block) : id(id), block(block) { }

    nodeid_t id;
    mempage block;
};

typedef boost::multi_index_container<
            CacheEntry,
            boost::multi_index::indexed_by<
                boost::multi_index::ordered_unique<boost::multi_index::member<CacheEntry, nodeid_t, &CacheEntry::id> >,
                boost::multi_index::sequenced<>
            > > cache_t;

typedef cache_t::nth_index<0>::type map_t;
typedef cache_t::nth_index<1>::type list_T;

/**
 * Block cache for block engines that need to traverse the network
 */
class BlockCache
{
public:
    BlockCache(size_t maxSize);

    bool get(nodeid_t id, mempage *mem);
    void put(nodeid_t id, const mempage &mem);
    void del(nodeid_t id);
private:
    cache_t m_cache;
    size_t m_maxSize;
    size_t m_size;
};

}}

#endif
