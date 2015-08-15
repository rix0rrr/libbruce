#include <libbruce/be/mem.h>

namespace bruce { namespace be {

mem::mem()
    : m_ctr(0)
{
}

mem::~mem()
{
}

blockid mem::newIdentifier()
{
    return m_ctr++;
}

range mem::get(const blockid &id)
{
    blockmap::iterator i = m_blocks.find(id);
    if (i == m_blocks.end()) throw block_not_found(id);
    return range(i->second.data(), i->second.size());
}

void mem::put(const blockid &id, const range &b)
{
    m_blocks[id].assign(b.size(), *b.byte_ptr());
}

void mem::del(const blockid &id)
{
    blockmap::iterator i = m_blocks.find(id);
    if (i != m_blocks.end()) m_blocks.erase(i);
}

}}
