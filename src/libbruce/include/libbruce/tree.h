#ifndef BRUCE_TREE_H
#define BRUCE_TREE_H

#include <vector>
#include <libbruce/mutation.h>
#include <libbruce/be/be.h>
#include <libbruce/types.h>
#include <boost/optional.hpp>

namespace bruce {

typedef boost::optional<be::blockid> maybe_blockid;

/**
 * Type-unsafe tree
 *
 * This class is an implementation detail.
 */
struct unsafe_tree
{
    unsafe_tree(const maybe_blockid &id, be::be &be, types::comparison_fn *compareKeys);

    mutation insert(const range &key, const range &value);
private:
    maybe_blockid m_id;
    be::be &m_be;
    types::comparison_fn *m_compareKeys;
};

/**
 * Typesafe tree
 */
template<typename K, typename V>
struct tree
{
    tree(const maybe_blockid &id, be::be &be)
        : m_unsafe(id, be, types::convert<K>::compare) { }

    mutation insert(K key, V value)
    {
        return m_unsafe.insert(types::convert<K>::to_bytes(key), types::convert<V>::to_bytes(value));
    }

private:
    unsafe_tree m_unsafe;
};


}

#endif
