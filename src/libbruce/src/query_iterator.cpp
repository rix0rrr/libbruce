#include <libbruce/query_iterator.h>

#include "query_iterator_impl.h"

namespace bruce {

query_iterator_unsafe::query_iterator_unsafe(query_iterator_impl_ptr impl)
    : m_impl(impl)
{
}

const memory &query_iterator_unsafe::key() const
{
    return m_impl->key();
}

const memory &query_iterator_unsafe::value() const
{
    return m_impl->value();
}

itemcount_t query_iterator_unsafe::rank() const
{
    return m_impl->rank();
}

bool query_iterator_unsafe::valid() const
{
    return m_impl && m_impl->valid();
}

void query_iterator_unsafe::skip(itemcount_t n)
{
    m_impl->skip(n);
}

void query_iterator_unsafe::next()
{
    m_impl->next();
}

query_iterator_unsafe::operator bool() const
{
    return m_impl && m_impl->valid();
}

}
