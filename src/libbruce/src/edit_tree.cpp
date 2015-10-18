#include <libbruce/edit_tree.h>

#include "edit_tree_impl.h"

namespace libbruce {

edit_tree_unsafe::edit_tree_unsafe(const maybe_nodeid &id, be::be &be, mempool &mempool, tree_functions fns)
    : m_tree(new edit_tree_impl(be, id, mempool, fns))
{
}

void edit_tree_unsafe::insert(const memslice &key, const memslice &value)
{
    m_tree->insert(key, value, false);
}

void edit_tree_unsafe::upsert(const memslice &key, const memslice &value)
{
    m_tree->insert(key, value, true);
}

bool edit_tree_unsafe::remove(const memslice &key)
{
    return m_tree->remove(key);
}

bool edit_tree_unsafe::remove(const memslice &key, const memslice &value)
{
    return m_tree->remove(key, value);
}

mutation edit_tree_unsafe::flush()
{
    return m_tree->flush();
}

}
