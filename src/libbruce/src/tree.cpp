#include <libbruce/tree.h>

#include "tree_impl.h"
#include "query_iterator_impl.h"

namespace libbruce {

tree_unsafe::tree_unsafe(const maybe_nodeid &id, be::be &be, mempool &mempool, tree_functions fns)
    : m_impl(new tree_impl(be, id, mempool, fns))
{
}

void tree_unsafe::insert(const memslice &key, const memslice &value)
{
    m_impl->insert(key, value);
}

void tree_unsafe::upsert(const memslice &key, const memslice &value, bool guaranteed)
{
    m_impl->upsert(key, value, guaranteed);
}

void tree_unsafe::remove(const memslice &key, bool guaranteed)
{
    m_impl->remove(key, guaranteed);
}

void tree_unsafe::remove(const memslice &key, const memslice &value, bool guaranteed)
{
    m_impl->remove(key, value, guaranteed);
}

mutation tree_unsafe::write()
{
    return m_impl->write();
}

bool tree_unsafe::get(const memslice &key, memslice *value)
{
    return m_impl->get(key, value);
}

query_iterator_unsafe tree_unsafe::find(const memslice &key)
{
    return query_iterator_unsafe(m_impl->find(key));
}

query_iterator_unsafe tree_unsafe::seek(itemcount_t n)
{
    return query_iterator_unsafe(m_impl->seek(n));
}

query_iterator_unsafe tree_unsafe::begin()
{
    return query_iterator_unsafe(m_impl->begin());
}

query_iterator_unsafe tree_unsafe::end()
{
    // Return an invalid iterator
    return query_iterator_unsafe(query_iterator_impl_ptr());
}

}

