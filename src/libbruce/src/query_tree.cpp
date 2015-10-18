#include <libbruce/query_tree.h>
#include "query_tree_impl.h"

namespace libbruce {


query_tree_unsafe::query_tree_unsafe(nodeid_t id, be::be &be, mempool &mempool, const tree_functions &fns)
    : m_impl(new query_tree_impl(be, id, mempool, fns))
{
}

void query_tree_unsafe::queue_insert(const memslice &key, const memslice &value)
{
    m_impl->queue_insert(key, value);
}

void query_tree_unsafe::queue_upsert(const memslice &key, const memslice &value, bool guaranteed)
{
    m_impl->queue_upsert(key, value, guaranteed);
}

void query_tree_unsafe::queue_remove(const memslice &key, bool guaranteed)
{
    m_impl->queue_remove(key, guaranteed);
}

void query_tree_unsafe::queue_remove(const memslice &key, const memslice &value, bool guaranteed)
{
    m_impl->queue_remove(key, value, guaranteed);
}

bool query_tree_unsafe::get(const memslice &key, memslice *value)
{
    return m_impl->get(key, value);
}

query_iterator_unsafe query_tree_unsafe::find(const memslice &key)
{
    return query_iterator_unsafe(m_impl->find(key));
}

query_iterator_unsafe query_tree_unsafe::seek(itemcount_t n)
{
    return query_iterator_unsafe(m_impl->seek(n));
}

query_iterator_unsafe query_tree_unsafe::begin()
{
    return query_iterator_unsafe(m_impl->begin());
}

query_iterator_unsafe query_tree_unsafe::end()
{
    // Return an invalid iterator
    return query_iterator_unsafe(query_iterator_impl_ptr());
}

}
