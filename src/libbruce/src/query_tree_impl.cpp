#include "query_tree_impl.h"

#include <cassert>
#include <list>

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
    rootPath.push_back(knuckle(root(), 0, memory(), memory()));

    query_iterator_impl_ptr it;
    findRec(rootPath, &key, &it);
    return it;
}

query_iterator_impl_ptr query_tree_impl::seek(itemcount_t n)
{
    std::vector<knuckle> rootPath;
    rootPath.push_back(knuckle(root(), 0, memory(), memory()));

    query_iterator_impl_ptr it;
    seekRec(rootPath, n, &it);
    return it;
}

void query_tree_impl::pushChildKnuckle(std::vector<knuckle> &rootPath)
{
    knuckle &top = rootPath.back();
    internalnode_ptr internal = boost::static_pointer_cast<InternalNode>(top.node);

    const memory &minK = internal->branch(top.index).minKey.size() ? internal->branch(top.index).minKey : top.minKey;
    const memory &maxK = top.index < internal->branchCount() - 1 ? internal->branch(top.index+1).minKey : top.maxKey;

    rootPath.push_back(knuckle(child(internal->branch(top.index)), 0, minK, maxK));
}

void query_tree_impl::findRec(std::vector<knuckle> &rootPath, const memory *key, query_iterator_impl_ptr *iter_ptr)
{
    knuckle &top = rootPath.back();
    node_ptr node = rootPath.back().node;

NODE_CASE_LEAF
    applyPendingChanges(top.minKey, top.maxKey);

    index_range keyrange = key ? findLeafRange(leaf, *key) : index_range(0, 1);

    for (top.index = keyrange.start; top.index < keyrange.end; top.index++)
    {
        int rank = 0;
        iter_ptr->reset(new query_iterator_impl(shared_from_this(), rootPath, rank));
        return;
    }

    // If it's not in here, it's definitely also not in the overflow node

NODE_CASE_OVERFLOW
    assert(false);

NODE_CASE_INT
    top.index = key ? FindInternalKey(internal, *key, m_fns) : 0;
    pushChildKnuckle(rootPath);
    findRec(rootPath, key, iter_ptr);

NODE_CASE_END
}

/**
 * Whether the given edit is guaranteed and does not need to be applied.
 *
 * An edit is guaranteed if it is itself guaranteed and NOT followed by a non-guaranteed edit (if
 * there is a later nonguaranteed edit, that one must be applied, and so all changes before it also
 * must be applied, not just counted).
 */
bool query_tree_impl::isGuaranteed(const editlist_t::iterator &cur, const editlist_t::iterator &end)
{
    for (editlist_t::iterator it = cur; it != end; ++it)
        if (!it->guaranteed)
            return false;
    return true;
}

void query_tree_impl::seekRec(std::vector<knuckle> &rootPath, itemcount_t n, query_iterator_impl_ptr *iter_ptr)
{
    knuckle &top = rootPath.back();
    node_ptr node = rootPath.back().node;

NODE_CASE_LEAF
    applyPendingChanges(top.minKey, top.maxKey);

    if (n < leaf->pairCount())
    {
        top.index = n;
        int rank = 0;
        iter_ptr->reset(new query_iterator_impl(shared_from_this(), rootPath, rank));
        return;
    }

NODE_CASE_OVERFLOW
    assert(false);

NODE_CASE_INT
    top.index = 0;

    while (top.index < internal->branchCount())
    {
        // Look for pending changes to apply here

        // Find bounds
        const memory &minK = top.index == 0 ? top.minKey : internal->branch(top.index).minKey;
        const memory &maxK = top.index < internal->branchCount() - 1 ? internal->branch(top.index + 1).minKey : top.maxKey;

        // Find edits
        editmap_t::iterator start = minK.empty() ? m_edits.begin() : m_edits.lower_bound(minK);
        editmap_t::iterator end = maxK.empty() ? m_edits.end() : m_edits.lower_bound(maxK);

        // Apply edits
        long int delta = 0;
        for (editmap_t::iterator kit = start; kit != end; /* increment below */)
        {
            for (editlist_t::iterator it = kit->second.begin(); it != kit->second.end(); )
            {
                // If guaranteed, only need to update a count, else really apply the change.
                // If the change was applied, remove it from the list.
                if (isGuaranteed(it, kit->second.end()))
                {
                    delta += it->delta();
                    ++it;
                }
                else
                {
                    applyPendingChangeRec(node, *it);
                    kit->second.erase(it);
                }
            }

            // Erase key iff the list is now empty
            if (kit->second.empty())
                m_edits.erase(kit++);
            else
                ++kit;
        }

        if (internal->branch(top.index).itemCount + delta <= n)
        {
            n -= internal->branch(top.index).itemCount;
            top.index++;
        }
        else {
            // Found where to descend
            pushChildKnuckle(rootPath);
            seekRec(rootPath, n, iter_ptr);
            return;
        }
    }

NODE_CASE_END
}

itemcount_t query_tree_impl::rank(const std::vector<knuckle> &rootPath)
{
    return 0;
}

query_iterator_impl_ptr query_tree_impl::begin()
{
    std::vector<knuckle> rootPath;
    rootPath.push_back(knuckle(root(), 0, memory(), memory()));
    query_iterator_impl_ptr it;
    findRec(rootPath, NULL, &it);
    return it;
}

void query_tree_impl::applyPendingChanges(const memory &minKey, const memory &maxKey)
{
    editmap_t::iterator start = minKey.empty() ? m_edits.begin() : m_edits.lower_bound(minKey);
    editmap_t::iterator end = maxKey.empty() ? m_edits.end() : m_edits.lower_bound(maxKey);

    for (editmap_t::iterator kit = start; kit != end; /* increment as part of erase */)
    {
        for (editlist_t::iterator it = kit->second.begin(); it != kit->second.end(); ++it)
            applyPendingChangeRec(root(), *it);
        m_edits.erase(kit++); // This is the idiomatic way to iterate+erase
    }
}

void query_tree_impl::applyPendingChangeRec(const node_ptr &node, const pending_edit &edit)
{
NODE_CASE_LEAF
    // We only need to apply changes to leaf nodes. No need to split into overflow blocks since
    // we're never going to write this back.
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

NODE_CASE_INT
    // Drill down to leaf, only to update counts afterwards
    keycount_t i = FindInternalKey(internal, edit.key, m_fns);
    node_ptr c = child(internal->branch(i));
    applyPendingChangeRec(c, edit);
    internal->branch(i).itemCount = c->itemCount();

NODE_CASE_END
}


}
