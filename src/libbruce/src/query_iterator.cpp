#include <libbruce/query_iterator.h>

#include "query_iterator_impl.h"

namespace bruce {

query_iterator_unsafe::query_iterator_unsafe(query_iterator_impl_ptr impl)
    : m_impl(impl)
{
}

// Copy constructor and assignment operator
// Don't just copy the ptr, copy the object INSIDE the ptr
query_iterator_unsafe::query_iterator_unsafe(const query_iterator_unsafe &rhs)
    : m_impl(new query_iterator_impl(*rhs.m_impl))
{
}

query_iterator_unsafe &query_iterator_unsafe::operator=(const query_iterator_unsafe &rhs)
{
    m_impl.reset(new query_iterator_impl(*rhs.m_impl));
    return *this;
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
