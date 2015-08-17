#include <libbruce/be/mem.h>

namespace bruce { namespace be {

mem::mem()
    : m_ctr(0)
{
}

mem::~mem()
{
}

nodeident_t mem::newIdentifier()
{
    return m_ctr++;
}

memory mem::get(const nodeident_t &id)
{
    blockmap::iterator i = m_blocks.find(id);
    if (i == m_blocks.end()) throw block_not_found(id);
    return memory(i->second.data(), i->second.size());
}

void mem::put(const nodeident_t &id, const memory &b)
{
    m_blocks[id].assign(b.size(), *b.byte_ptr());
}

void mem::del(const nodeident_t &id)
{
    blockmap::iterator i = m_blocks.find(id);
    if (i != m_blocks.end()) m_blocks.erase(i);
}

}}
