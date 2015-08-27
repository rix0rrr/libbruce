/**
 * Tree interface
 *
 * To avoid having the entire implementation in headers, yet wanting to use
 * stack-allocated classes for grouping state and templates for type safety,
 * the implementation is in 3 layers:
 *
 * - Consumers interact with a `tree<K, V>` which is typed and the type traits
 *   are defined in <libbruce/types.h>.
 * - Methods of this class delegate in the header to a class named
 *   `unsafe_tree`, which has lost the notion of types and works only at the
 *   level of byte streams.
 * - The methods of `unsafe_tree` can now be implemented in the source file as
 *   opposed to in the header. However, the actual work is delegated again to
 *   implementation-internal classes in `src/operation.*`, so we can group
 *   variables related to the operation in a class without needing to expose
 *   the implementation details in this header.
 */
#ifndef BRUCE_TREE_H
#define BRUCE_TREE_H

#include <vector>
#include <libbruce/mutation.h>
#include <libbruce/be/be.h>
#include <libbruce/traits.h>
#include <boost/optional.hpp>

namespace bruce {

typedef boost::optional<nodeid_t> maybe_blockid;

/**
 * Type-unsafe tree
 *
 * This class is an implementation detail and should not be used directly.
 */
struct unsafe_tree
{
    unsafe_tree(const maybe_blockid &id, be::be &be, tree_functions fns);

    mutation insert(const memory &key, const memory &value);
private:
    maybe_blockid m_id;
    be::be &m_be;
    tree_functions m_fns;
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
