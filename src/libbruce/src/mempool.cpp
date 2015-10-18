#include <libbruce/mempool.h>

#define ALLOCPAGE_SIZE (256 * 1024)

// Requests of this size will never be satisfied from a pool page, but from
// a direct malloc allocation.
#define MAX_SHARED_ALLOC_SIZE (ALLOCPAGE_SIZE / 4)

namespace libbruce {

mempool::mempool()
    : m_allocOffset(0)
{
}

void mempool::retain(const mempage &page)
{
    m_pages.push_back(page);
}

memslice mempool::alloc(size_t size)
{
    if (MAX_SHARED_ALLOC_SIZE < size)
    {
        retain(mempage(size));
        return m_pages.back().all();
    }

    if (m_allocPage.size() <= m_allocOffset + size)
    {
        if (!m_allocPage.empty()) retain(m_allocPage);
        m_allocPage = mempage(ALLOCPAGE_SIZE);
        m_allocOffset = 0;
    }

    size_t offset = m_allocOffset;
    m_allocOffset += size;

    return m_allocPage.slice(offset, size);
}

}
