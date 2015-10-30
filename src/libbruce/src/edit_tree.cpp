#include <libbruce/edit_tree.h>

#include "tree_impl.h"

namespace libbruce {

edit_tree_unsafe::edit_tree_unsafe(const maybe_nodeid &id, be::be &be, mempool &mempool, tree_functions fns)
    : m_tree(new tree_impl(be, id, mempool, fns))
{
}

void edit_tree_unsafe::insert(const memslice &key, const memslice &value)
{
    m_tree->insert(key, value);
}

void edit_tree_unsafe::upsert(const memslice &key, const memslice &value, bool guaranteed)
{
    m_tree->upsert(key, value, guaranteed);
}

void edit_tree_unsafe::remove(const memslice &key, bool guaranteed)
{
    m_tree->remove(key, guaranteed);
}

void edit_tree_unsafe::remove(const memslice &key, const memslice &value, bool guaranteed)
{
    m_tree->remove(key, value, guaranteed);
}

mutation edit_tree_unsafe::flush()
{
    return m_tree->flush();
}

}
