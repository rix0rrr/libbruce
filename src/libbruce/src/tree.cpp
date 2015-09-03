#include <libbruce/tree.h>

#include "operations.h"

namespace bruce {

unsafe_tree::unsafe_tree(const maybe_nodeid &id, be::be &be, tree_functions fns)
    : m_tree(new mutable_tree(be, id, fns))
{
}

void unsafe_tree::insert(const memory &key, const memory &value)
{
    m_tree->insert(key, value);
}

bool unsafe_tree::remove(const memory &key)
{
    return m_tree->remove(key);
}

bool unsafe_tree::remove(const memory &key, const memory &value)
{
    return m_tree->remove(key, value);
}

mutation unsafe_tree::flush()
{
    return m_tree->flush();
}

}
