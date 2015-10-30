/**
 * Tree interface
 *
 * To avoid having the entire implementation in headers, yet wanting to use
 * stack-allocated classes for grouping state and templates for type safety,
 * the implementation is in 3 layers:
 *
 * - Consumers interact with a `tree<K, V>` which is typed and the type traits
 *   are defined in <libbruce/traits.h>.
 * - Methods of this class delegate in the header to a class named
 *   `unsafe_tree`, which has lost the notion of types and works only at the
 *   level of byte streams.
 * - The methods of `unsafe_tree` can now be implemented in the source file as
 *   opposed to in the header. However, the actual work is delegated again to
 *   implementation-internal classes in `src/operations.*`, so we can group
 *   variables related to the operation in a class without needing to expose
 *   the implementation details in this header.
 */
#pragma once
#ifndef BRUCE_TREE_H
#define BRUCE_TREE_H

#include <vector>
#include <libbruce/mutation.h>
#include <libbruce/be/be.h>
#include <libbruce/traits.h>
#include <boost/make_shared.hpp>

#include <libbruce/query_iterator.h>

namespace libbruce {

/**
 * Type-unsafe tree
 *
 * This class is an implementation detail and should not be used directly.
 */
struct tree_unsafe
{
    tree_unsafe(const maybe_nodeid &id, be::be &be, mempool &mempool, tree_functions fns);

    void insert(const memslice &key, const memslice &value);
    void upsert(const memslice &key, const memslice &value, bool guaranteed);
    void remove(const memslice &key, bool guaranteed);
    void remove(const memslice &key, const memslice &value, bool guaranteed);
    mutation write();

    bool get(const memslice &key, memslice *value);
    query_iterator_unsafe find(const memslice &key);
    query_iterator_unsafe seek(itemcount_t n);
    query_iterator_unsafe begin();
    query_iterator_unsafe end();
private:
    tree_impl_ptr m_impl;
};

/**
 * Typesafe tree
 */
template<typename K, typename V>
struct tree
{
    typedef typename boost::shared_ptr<tree<K, V> > ptr;
    typedef typename boost::optional<V> maybe_v;
    typedef query_iterator<K, V> iterator;

    tree(const maybe_nodeid &id, be::be &be)
        : m_unsafe(id, be, m_mempool, fns) { }

    void insert(const K &key, const V &value)
    {
        m_unsafe.insert(traits::convert<K>::to_bytes(key, m_mempool), traits::convert<V>::to_bytes(value, m_mempool));
    }

    void upsert(const K &key, const V &value, bool guaranteed)
    {
        m_unsafe.upsert(traits::convert<K>::to_bytes(key, m_mempool), traits::convert<V>::to_bytes(value, m_mempool), guaranteed);
    }

    void remove(const K &key, bool guaranteed)
    {
        m_unsafe.remove(traits::convert<K>::to_bytes(key, m_mempool), guaranteed);
    }

    void remove(const K &key, const V &value, bool guaranteed)
    {
        m_unsafe.remove(traits::convert<K>::to_bytes(key, m_mempool), traits::convert<V>::to_bytes(value, m_mempool), guaranteed);
    }

    mutation write()
    {
        return m_unsafe.write();
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

    static tree_functions fns;
private:
    tree_unsafe m_unsafe;
    mempool m_mempool;
};

template<typename K, typename V>
tree_functions tree<K, V>::fns(&traits::convert<K>::compare, &traits::convert<V>::compare, &traits::convert<K>::size, &traits::convert<V>::size);

}

#endif
