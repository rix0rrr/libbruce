#include <libbruce/tree_iterator.h>

#include "tree_iterator_impl.h"

namespace libbruce {

tree_iterator_unsafe::tree_iterator_unsafe()
{
}

tree_iterator_unsafe::tree_iterator_unsafe(const tree_iterator_impl_ptr &impl)
    : m_impl(impl)
{
}

// Copy constructor and assignment operator
// Don't just copy the ptr, copy the object INSIDE the ptr
tree_iterator_unsafe::tree_iterator_unsafe(const tree_iterator_unsafe &rhs)
    : m_impl(rhs.m_impl ? new tree_iterator_impl(*rhs.m_impl) : NULL)
{
}

tree_iterator_unsafe &tree_iterator_unsafe::operator=(const tree_iterator_unsafe &rhs)
{
    m_impl.reset(rhs.m_impl ? new tree_iterator_impl(*rhs.m_impl) : NULL);
    return *this;
}

const memslice &tree_iterator_unsafe::key() const
{
    checkValid();
    return m_impl->key();
}

const memslice &tree_iterator_unsafe::value() const
{
    checkValid();
    return m_impl->value();
}

itemcount_t tree_iterator_unsafe::rank() const
{
    checkValid();
    return m_impl->rank();
}

void tree_iterator_unsafe::checkValid() const
{
    if (!valid()) throw std::runtime_error("Iterator is not valid");
}

bool tree_iterator_unsafe::valid() const
{
    return m_impl && m_impl->valid();
}

void tree_iterator_unsafe::skip(int n)
{
    checkValid();
    m_impl->skip(n);
}

void tree_iterator_unsafe::next()
{
    checkValid();
    m_impl->next();
}

tree_iterator_unsafe::operator bool() const
{
    return m_impl && m_impl->valid();
}

bool tree_iterator_unsafe::operator==(const tree_iterator_unsafe &other) const
{
    if (valid() != other.valid()) return false;
    if (!valid()) return true;
    return *m_impl == *other.m_impl;
}

}
