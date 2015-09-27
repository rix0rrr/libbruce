#include <libbruce/query_iterator.h>

#include "query_iterator_impl.h"

namespace libbruce {

query_iterator_unsafe::query_iterator_unsafe()
{
}

query_iterator_unsafe::query_iterator_unsafe(query_iterator_impl_ptr impl)
    : m_impl(impl)
{
}

// Copy constructor and assignment operator
// Don't just copy the ptr, copy the object INSIDE the ptr
query_iterator_unsafe::query_iterator_unsafe(const query_iterator_unsafe &rhs)
    : m_impl(rhs.m_impl ? new query_iterator_impl(*rhs.m_impl) : NULL)
{
}

query_iterator_unsafe &query_iterator_unsafe::operator=(const query_iterator_unsafe &rhs)
{
    m_impl.reset(rhs.m_impl ? new query_iterator_impl(*rhs.m_impl) : NULL);
    return *this;
}

const memory &query_iterator_unsafe::key() const
{
    checkValid();
    return m_impl->key();
}

const memory &query_iterator_unsafe::value() const
{
    checkValid();
    return m_impl->value();
}

itemcount_t query_iterator_unsafe::rank() const
{
    checkValid();
    return m_impl->rank();
}

void query_iterator_unsafe::checkValid() const
{
    if (!valid()) throw std::runtime_error("Iterator is not valid");
}

bool query_iterator_unsafe::valid() const
{
    return m_impl && m_impl->valid();
}

void query_iterator_unsafe::skip(int n)
{
    checkValid();
    m_impl->skip(n);
}

void query_iterator_unsafe::next()
{
    checkValid();
    m_impl->next();
}

query_iterator_unsafe::operator bool() const
{
    return m_impl && m_impl->valid();
}

bool query_iterator_unsafe::operator==(const query_iterator_unsafe &other) const
{
    if (valid() != other.valid()) return false;
    if (!valid()) return true;
    return *m_impl == *other.m_impl;
}

}
