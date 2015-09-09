#include <libbruce/query_tree.h>
#include "query_tree_impl.h"

namespace bruce {


query_tree_unsafe::query_tree_unsafe(nodeid_t id, be::be &be, const tree_functions &fns)
    : m_impl(new query_tree_impl(id, be, fns))
{
}

void query_tree_unsafe::queue_insert(const memory &key, const memory &value)
{
}

void query_tree_unsafe::queue_remove(const memory &key, bool guaranteed)
{
}

void query_tree_unsafe::queue_remove(const memory &key, const memory &value, bool guaranteed)
{
}

bool query_tree_unsafe::get(const memory &key, memory *value)
{
    return false;
}

}

