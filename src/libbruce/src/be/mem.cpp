#include <libbruce/be/mem.h>

#include <boost/lexical_cast.hpp>

#define to_string boost::lexical_cast<std::string>

namespace libbruce { namespace be {

mem::mem(uint32_t maxBlockSize)
    : m_ctr(0), m_maxBlockSize(maxBlockSize)
{
}

mem::~mem()
{
}

nodeid_t mem::blockCount() const
{
    return m_blocks.size();
}

void mem::newIdentifiers(int n, std::vector<nodeid_t> *out)
{
    for (int i = 0; i < n; i++)
         out->push_back(m_ctr++);
}

memory mem::get(const nodeid_t &id)
{
    blockmap::iterator i = m_blocks.find(id);
    if (i == m_blocks.end()) throw block_not_found(id);
    return i->second;
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
        blockmap::iterator i = m_blocks.find(it->id);
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

}}
