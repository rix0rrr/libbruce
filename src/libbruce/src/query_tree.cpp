#include <libbruce/query_tree.h>
#include "query_tree_impl.h"

namespace bruce {


query_tree_unsafe::query_tree_unsafe(nodeid_t id, be::be &be, const tree_functions &fns)
    : m_impl(new query_tree_impl(be, id, fns))
{
}

void query_tree_unsafe::queue_insert(const memory &key, const memory &value)
{
    m_impl->queue_insert(key, value);
}

void query_tree_unsafe::queue_remove(const memory &key, bool guaranteed)
{
    m_impl->queue_remove(key, guaranteed);
}

void query_tree_unsafe::queue_remove(const memory &key, const memory &value, bool guaranteed)
{
    m_impl->queue_remove(key, value, guaranteed);
}

bool query_tree_unsafe::get(const memory &key, memory *value)
{
    return m_impl->get(key, value);
}

query_iterator_unsafe query_tree_unsafe::find(const memory &key)
{
    return query_iterator_unsafe(m_impl->find(key));
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
