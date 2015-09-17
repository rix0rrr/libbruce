#include "query_tree_impl.h"

#include <cassert>

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
    query_iterator_unsafe it = find(key);
    if (it) *value = it.value();
    return it;
}

query_iterator_impl_ptr query_tree_impl::find(const memory &key)
{
    std::vector<knuckle> rootPath;
    query_iterator_impl_ptr it;
    findRec(root(), key, memory(), memory(), rootPath, &it);
    return it;
}

bool query_tree_impl::findRec(const node_ptr &node, const memory &key, const memory &minKey, const memory &maxKey, std::vector<knuckle> &rootPath, query_iterator_impl_ptr *iter_ptr)
{
NODE_CASE_LEAF
    applyPendingChanges(leaf, minKey, maxKey);

    index_range keyrange = findLeafRange(leaf, key);

    for (keycount_t i = keyrange.start; i < keyrange.end; i++)
    {
        int rank = 0;
        rootPath.push_back(knuckle(node, i));
        iter_ptr->reset(new query_iterator_impl(shared_from_this(), rootPath, rank));
        return true;
    }

    // If it's not in here, it's definitely also not in the overflow node
    return false;

NODE_CASE_OVERFLOW
    assert(false);
    return false;

NODE_CASE_INT
    keycount_t i = FindInternalKey(internal, key, m_fns);

    rootPath.push_back(knuckle(node, i));

    const memory &minK = internal->branch(i).minKey.size() ? internal->branch(i).minKey : minKey;
    const memory &maxK = i < internal->branchCount() - 1 ? internal->branch(i+1).minKey : maxKey;

    if (findRec(child(internal->branch(i)), key, minK, maxK, rootPath, iter_ptr))
        return true;

    return false;

NODE_CASE_END
}

void query_tree_impl::applyPendingChanges(const node_ptr &node, const memory &minKey, const memory &maxKey)
{
NODE_CASE_LEAF
    // We only need to apply changes to leaf nodes. No need to split into overflow blocks since
    // we're never going to write this back.
    editmap_t::iterator start = minKey.empty() ? m_edits.begin() : m_edits.lower_bound(minKey);
    editmap_t::iterator end = maxKey.empty() ? m_edits.end() : m_edits.lower_bound(maxKey);
    for (editmap_t::iterator kit = start; kit != end; /* increment as part of erase */)
    {
        for (editlist_t::iterator it = kit->second.begin(); it != kit->second.end(); ++it)
            applyPendingChange(leaf, *it);

        m_edits.erase(kit++); // This is the idiomatic way to iterate+erase
    }

NODE_CASE_END
}

void query_tree_impl::applyPendingChange(const leafnode_ptr &leaf, const pending_edit &edit)
{
    switch (edit.edit)
    {
        case INSERT:
            {
                keycount_t i = FindLeafKey(leaf, edit.key, m_fns);
                leaf->insert(i, kv_pair(edit.key, edit.value));
            }
            break;
        case REMOVE_KEY:
            removeFromLeaf(leaf, edit.key, NULL);
            break;
        case REMOVE_KV:
            removeFromLeaf(leaf, edit.key, &edit.value);
            break;
        default:
            assert(false);
            throw std::runtime_error("Unrecognized queued command.");
    };
}


}
