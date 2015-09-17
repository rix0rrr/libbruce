#ifndef BRUCE_QUERY_ITERATOR_H
#define BRUCE_QUERY_ITERATOR_H

#include <libbruce/be/be.h>
#include <libbruce/traits.h>
#include <boost/make_shared.hpp>

namespace bruce {

class query_iterator_impl;
typedef boost::shared_ptr<query_iterator_impl> query_iterator_impl_ptr;

struct query_iterator_unsafe
{
    query_iterator_unsafe(query_iterator_impl_ptr impl);

    const memory &key() const;
    const memory &value() const;
    itemcount_t rank() const;
    bool valid() const;

    void skip(itemcount_t n);
    void next();

    operator bool() const;
private:
    query_iterator_impl_ptr m_impl;
};

template<typename K, typename V>
struct query_iterator
{
    query_iterator(query_iterator_unsafe unsafe) : m_unsafe(unsafe) { }

    V value() const { return traits::convert<V>::from_bytes(m_unsafe.value()); }
    K key() const { return traits::convert<K>::from_bytes(m_unsafe.key()); }
    itemcount_t rank() const { return m_unsafe.rank(); }
    void skip(itemcount_t n) { m_unsafe.skip(n); }

    query_iterator &operator++() { m_unsafe.next(); return *this; } // Prefix
    query_iterator operator++(int) { query_iterator<K, V> ret(*this); m_unsafe.next(); return ret; } // Postfix
    void operator+=(itemcount_t n) { skip(n); }
    operator bool() const { return m_unsafe.valid(); }
private:
    query_iterator_unsafe m_unsafe;
};

}

#endif
