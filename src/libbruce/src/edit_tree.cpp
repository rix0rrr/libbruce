#include <libbruce/edit_tree.h>

#include "edit_tree_impl.h"

namespace bruce {

edit_tree_unsafe::edit_tree_unsafe(const maybe_nodeid &id, be::be &be, tree_functions fns)
    : m_tree(new edit_tree_impl(be, id, fns))
{
}

void edit_tree_unsafe::insert(const memory &key, const memory &value)
{
    m_tree->insert(key, value, false);
}

void edit_tree_unsafe::upsert(const memory &key, const memory &value)
{
    m_tree->insert(key, value, true);
}

bool edit_tree_unsafe::remove(const memory &key)
{
    return m_tree->remove(key);
}

bool edit_tree_unsafe::remove(const memory &key, const memory &value)
{
    return m_tree->remove(key, value);
}

mutation edit_tree_unsafe::flush()
{
    return m_tree->flush();
}

}
