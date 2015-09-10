#include "query_tree_impl.h"

namespace bruce {

query_tree_impl::query_tree_impl(be::be &be, nodeid_t rootID, const tree_functions &fns)
    : tree_impl(be, rootID, fns), m_edits(callback_memcmp(fns))
{
}

void query_tree_impl::queue_insert(const memory &key, const memory &value)
{
    m_edits[key].push_back(pending_edit(INSERT, key, value, true));
}

void query_tree_impl::queue_remove(const memory &key, bool guaranteed)
{
    m_edits[key].push_back(pending_edit(REMOVE_KEY, key, memory(), guaranteed));
}

void query_tree_impl::queue_remove(const memory &key, const memory &value, bool guaranteed)
{
    m_edits[key].push_back(pending_edit(REMOVE_KV, key, value, guaranteed));
}

bool query_tree_impl::get(const memory &key, memory *value)
{
    return getRec(root(), key, value);
}

bool query_tree_impl::getRec(const node_ptr &node, const memory &key, memory *value)
{
NODE_CASE_LEAF
    index_range keyrange = findLeafRange(leaf, key);

    for (keycount_t i = keyrange.start; i < keyrange.end; i++)
    {
        *value = leaf->pair(i).value;
        return true;
    }
    return false;

NODE_CASE_INT
    index_range keyrange = findInternalRange(internal, key);

    for (keycount_t i = keyrange.start; i < keyrange.end; i++)
    {
        if (getRec(child(internal->branch(i)), key, value))
            return true;
    }
    return false;

NODE_CASE_END
}


}
