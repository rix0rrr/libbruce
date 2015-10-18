#pragma once
#ifndef BRUCE_QUERY_H
#define BRUCE_QUERY_H

#include <libbruce/be/be.h>
#include <libbruce/traits.h>
#include <libbruce/query_iterator.h>
#include <boost/make_shared.hpp>

namespace libbruce {

class query_tree_impl;
typedef boost::shared_ptr<query_tree_impl> query_tree_impl_ptr;

struct query_tree_unsafe
{
    query_tree_unsafe(nodeid_t id, be::be &be, mempool &mempool, const tree_functions &fns);

    void queue_insert(const memslice &key, const memslice &value);
    void queue_upsert(const memslice &key, const memslice &value, bool guaranteed);
    void queue_remove(const memslice &key, bool guaranteed);
    void queue_remove(const memslice &key, const memslice &value, bool guaranteed);

    bool get(const memslice &key, memslice *value);
    query_iterator_unsafe find(const memslice &key);
    query_iterator_unsafe seek(itemcount_t n);
    query_iterator_unsafe begin();
    query_iterator_unsafe end();
private:
    query_tree_impl_ptr m_impl;
};

template<typename K, typename V>
struct query_tree
{
    typedef typename boost::shared_ptr<query_tree<K, V> > ptr;
    typedef typename boost::optional<V> maybe_v;
    typedef query_iterator<K, V> iterator;

    query_tree(nodeid_t id, be::be &be)
        : m_unsafe(id, be, m_mempool, fns()) { }

    void queue_insert(const K &key, const V &value)
    {
        m_unsafe.queue_insert(traits::convert<K>::to_bytes(key, m_mempool), traits::convert<V>::to_bytes(value, m_mempool));
    }

    void queue_upsert(const K &key, const V &value, bool guaranteed)
    {
        m_unsafe.queue_upsert(traits::convert<K>::to_bytes(key, m_mempool), traits::convert<V>::to_bytes(value, m_mempool), guaranteed);
    }

    void queue_remove(const K &key, bool guaranteed)
    {
        m_unsafe.queue_remove(traits::convert<K>::to_bytes(key, m_mempool), guaranteed);
    }

    void queue_remove(const K &key, const V &value, bool guaranteed)
    {
        m_unsafe.queue_remove(traits::convert<K>::to_bytes(key, m_mempool), traits::convert<V>::to_bytes(value, m_mempool), guaranteed);
    }

    maybe_v get(const K &key)
    {
        memslice value;
        if (m_unsafe.get(traits::convert<K>::to_bytes(key, m_mempool), &value))
            return traits::convert<V>::from_bytes(value);
        else
            return maybe_v();
    }

    iterator find(const K &key)
    {
        return query_iterator<K,V>(m_unsafe.find(traits::convert<K>::to_bytes(key, m_mempool)));
    }

    iterator seek(itemcount_t n)
    {
        return query_iterator<K,V>(m_unsafe.seek(n));
    }

    iterator begin()
    {
        return query_iterator<K,V>(m_unsafe.begin());
    }

    iterator end()
    {
        return query_iterator<K,V>(m_unsafe.end());
    }

    static tree_functions fns() { return tree_functions(&traits::convert<K>::compare, &traits::convert<V>::compare, &traits::convert<K>::size, &traits::convert<V>::size); }
private:
    query_tree_unsafe m_unsafe;
    mempool m_mempool;
};


}

#endif
