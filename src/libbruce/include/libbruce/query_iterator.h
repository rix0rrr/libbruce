#pragma once
#ifndef BRUCE_QUERY_ITERATOR_H
#define BRUCE_QUERY_ITERATOR_H

#include <libbruce/be/be.h>
#include <libbruce/traits.h>
#include <boost/make_shared.hpp>

namespace libbruce {

struct query_iterator_unsafe
{
    query_iterator_unsafe();
    query_iterator_unsafe(const query_iterator_impl_ptr &impl);
    query_iterator_unsafe(const query_iterator_unsafe &rhs);
    query_iterator_unsafe &operator=(const query_iterator_unsafe &rhs);

    const memslice &key() const;
    const memslice &value() const;
    itemcount_t rank() const;
    bool valid() const;

    void skip(int n);
    void next();

    operator bool() const;

    bool operator==(const query_iterator_unsafe &other) const;
    bool operator!=(const query_iterator_unsafe &other) const { return !(*this == other); }
private:
    query_iterator_impl_ptr m_impl;
    void checkValid() const;
};

template<typename K, typename V>
struct query_iterator
{
    query_iterator() {}
    query_iterator(query_iterator_unsafe unsafe) : m_unsafe(unsafe) { }

    V value() const { return traits::convert<V>::from_bytes(m_unsafe.value()); }
    K key() const { return traits::convert<K>::from_bytes(m_unsafe.key()); }
    itemcount_t rank() const { return m_unsafe.rank(); }
    void skip(int n) { m_unsafe.skip(n); }

    query_iterator &operator++() { m_unsafe.next(); return *this; } // Prefix
    query_iterator operator++(int) { query_iterator<K, V> ret(*this); m_unsafe.next(); return ret; } // Postfix
    void operator+=(int n) { skip(n); }
    operator bool() const { return m_unsafe.valid(); }
    bool operator==(const query_iterator<K, V> &other) const { return m_unsafe == other.m_unsafe; }
    bool operator!=(const query_iterator<K, V> &other) const { return !(*this == other); }
private:
    query_iterator_unsafe m_unsafe;
};

}

#endif
