#include <libbruce/be/mem.h>
#include <libbruce/util/be_registry.h>
#include <boost/make_shared.hpp>

#include <boost/lexical_cast.hpp>

#define to_string boost::lexical_cast<std::string>

namespace libbruce { namespace be {

mem::mem(uint32_t maxBlockSize, uint32_t editQueueSize)
    : m_ctr(0), m_maxBlockSize(maxBlockSize), m_editQueueSize(editQueueSize)
{
}

mem::~mem()
{
}

size_t mem::blockCount() const
{
    return m_blocks.size();
}

nodeid_t mem::id(const mempage &block)
{
    return nodeid_t(m_ctr++);
}

mempage mem::get(const nodeid_t &id)
{
    blockmap_t::iterator i = m_blocks.find(id);
    if (i == m_blocks.end()) throw block_not_found(id);
    return i->second;
}

getblockresult_t mem::get_all(const blockidlist_t &ids)
{
    getblockresult_t ret;
    for (blockidlist_t::const_iterator it = ids.begin(); it != ids.end(); ++it)
        ret[*it] = get(*it);
    return ret;
}

void mem::put_all(putblocklist_t &blocklist)
{
    for (putblocklist_t::iterator it = blocklist.begin(); it != blocklist.end(); ++it)
    {
        if (it->mem.size() > m_maxBlockSize)
            throw std::runtime_error((std::string("Block too large: ") + to_string(it->mem.size()) + " > " + to_string(m_maxBlockSize)).c_str());

        m_blocks[it->id] = it->mem;

        it->success = true;
    }
}

void mem::del_all(delblocklist_t &ids)
{
    for (delblocklist_t::iterator it = ids.begin(); it != ids.end(); ++it)
    {
        blockmap_t::iterator i = m_blocks.find(it->id);
        if (i != m_blocks.end()) {
            m_blocks.erase(i);
            it->success = true;
        }
    }
}

uint32_t mem::maxBlockSize()
{
    return m_maxBlockSize;
}

uint32_t mem::editQueueSize()
{
    return m_editQueueSize;
}

be_ptr create_mem_engine(const std::string &location, size_t block_size, size_t queue_size, const util::options_t &options)
{
    return boost::make_shared<mem>(block_size, queue_size);
}

void register_mem_engine()
{
    util::register_be_factory("mem", &create_mem_engine);
}

}}
