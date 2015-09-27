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
#ifndef BRUCE_TREE_H
#define BRUCE_TREE_H

#include <vector>
#include <libbruce/mutation.h>
#include <libbruce/be/be.h>
#include <libbruce/traits.h>
#include <boost/make_shared.hpp>

namespace libbruce {

class edit_tree_impl;
typedef boost::shared_ptr<edit_tree_impl> mutable_tree_ptr;

/**
 * Type-unsafe tree
 *
 * This class is an implementation detail and should not be used directly.
 */
struct edit_tree_unsafe
{
    edit_tree_unsafe(const maybe_nodeid &id, be::be &be, tree_functions fns);

    void insert(const memory &key, const memory &value);
    void upsert(const memory &key, const memory &value);
    bool remove(const memory &key);
    bool remove(const memory &key, const memory &value);
    mutation flush();
private:
    mutable_tree_ptr m_tree;
};

/**
 * Typesafe tree
 */
template<typename K, typename V>
struct edit_tree
{
    typedef typename boost::shared_ptr<edit_tree<K, V> > ptr;

    edit_tree(const maybe_nodeid &id, be::be &be)
        : m_unsafe(id, be, fns()) { }

    void insert(const K &key, const V &value)
    {
        m_unsafe.insert(traits::convert<K>::to_bytes(key), traits::convert<V>::to_bytes(value));
    }

    void upsert(const K &key, const V &value)
    {
        m_unsafe.upsert(traits::convert<K>::to_bytes(key), traits::convert<V>::to_bytes(value));
    }

    bool remove(const K &key)
    {
        return m_unsafe.remove(traits::convert<K>::to_bytes(key));
    }

    bool remove(const K &key, const V &value)
    {
        return m_unsafe.remove(traits::convert<K>::to_bytes(key), traits::convert<V>::to_bytes(value));
    }

    mutation flush()
    {
        return m_unsafe.flush();
    }

    static tree_functions fns() { return tree_functions(&traits::convert<K>::compare, &traits::convert<V>::compare, &traits::convert<K>::size, &traits::convert<V>::size); }
private:
    edit_tree_unsafe m_unsafe;
};

}

#endif
