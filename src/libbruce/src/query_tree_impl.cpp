#include "query_tree_impl.h"

#include <cassert>
#include <list>

namespace libbruce {

query_tree_impl::query_tree_impl(be::be &be, nodeid_t rootID, const tree_functions &fns)
    : tree_impl(be, rootID, fns), m_edits(callback_memcmp(fns))
{
}

void query_tree_impl::queue_insert(const memory &key, const memory &value)
{
    m_edits[key].push_back(pending_edit(INSERT, key, value, true));
}

void query_tree_impl::queue_upsert(const memory &key, const memory &value, bool guaranteed)
{
    m_edits[key].push_back(pending_edit(UPSERT, key, value, guaranteed));
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
    treepath_t rootPath;
    rootPath.push_back(knuckle(root(), memory(), memory()));

    query_iterator_impl_ptr it;
    findRec(rootPath, &key, &it);
    return it;
}

query_iterator_impl_ptr query_tree_impl::seek(itemcount_t n)
{
    treepath_t rootPath;
    rootPath.push_back(knuckle(root(), memory(), memory()));

    query_iterator_impl_ptr it;
    seekRec(rootPath, n, &it);
    return it;
}

void query_tree_impl::pushChildKnuckle(treepath_t &rootPath)
{
    knuckle &top = rootPath.back();
    internalnode_ptr internal = boost::static_pointer_cast<InternalNode>(top.node);

    const memory &minK = internal->branch(top.index).minKey.size() ? internal->branch(top.index).minKey : top.minKey;
    const memory &maxK = top.index < internal->branchCount() - 1 ? internal->branch(top.index+1).minKey : top.maxKey;

    rootPath.push_back(knuckle(child(internal->branch(top.index)), minK, maxK));
}

void query_tree_impl::findRec(treepath_t &rootPath, const memory *key, query_iterator_impl_ptr *iter_ptr)
{
    knuckle &top = rootPath.back();
    node_ptr node = rootPath.back().node;

NODE_CASE_LEAF
    applyPendingChanges(top.minKey, top.maxKey, NULL, NULL);

    pairlist_t::iterator begin = leaf->pairs.begin();
    pairlist_t::iterator end = leaf->pairs.end();
    if (key) findLeafRange(leaf, *key, &begin, &end);

    for (top.leafIter = begin; top.leafIter != end; ++top.leafIter)
    {
        iter_ptr->reset(new query_iterator_impl(shared_from_this(), rootPath));
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

void query_tree_impl::seekRec(treepath_t &rootPath, itemcount_t n, query_iterator_impl_ptr *iter_ptr)
{
    knuckle &top = rootPath.back();
    node_ptr node = rootPath.back().node;

NODE_CASE_LEAF
    applyPendingChanges(top.minKey, top.maxKey, NULL, NULL);

    if (n < leaf->pairCount())
    {
        top.leafIter = leaf->get_at(n);
        iter_ptr->reset(new query_iterator_impl(shared_from_this(), rootPath));
        return;
    }

    n -= leaf->pairCount();

    if (!leaf->overflow.empty())
    {
        rootPath.push_back(knuckle(overflowNode(leaf->overflow), memory(), memory()));
        seekRec(rootPath, n, iter_ptr);
    }

NODE_CASE_OVERFLOW

    if (n < overflow->valueCount())
    {
        top.index = n;
        iter_ptr->reset(new query_iterator_impl(shared_from_this(), rootPath));
        return;
    }

    n -= overflow->valueCount();

    if (!overflow->next.empty())
        rootPath.push_back(knuckle(overflowNode(overflow->next), memory(), memory()));
        seekRec(rootPath, n, iter_ptr);

NODE_CASE_INT
    top.index = 0;

    while (top.index < internal->branchCount())
    {
        // Look for pending changes to apply here

        // Find bounds
        const memory &minK = top.index == 0 ? top.minKey : internal->branch(top.index).minKey;
        const memory &maxK = top.index < internal->branchCount() - 1 ? internal->branch(top.index + 1).minKey : top.maxKey;

        // Find edits
        int delta = pendingRankDelta(node, minK, maxK);

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

itemcount_t query_tree_impl::rank(treepath_t &rootPath)
{
    itemcount_t ret = 0;
    for (treepath_t::iterator it = rootPath.begin(); it != rootPath.end(); ++it)
    {
        node_ptr node = it->node;

    NODE_CASE_LEAF
        // This WILL change the index of dude. Need to shift it to stay on the same element.
        int delta = 0;
        const memory &maxK = it->index < leaf->pairCount() ? it->leafIter->first : it->maxKey;

        // FIXME: Shouldn't the next applyPendingChanges() always apply 0 changes?
        applyPendingChanges(it->minKey, maxK, &delta, &it->leafIter); // Apply up to this key

        // Unfortunately, we need to do linear search here.
        itemcount_t index = 0;
        for (pairlist_t::const_iterator i = leaf->pairs.begin(); i != it->leafIter; ++i)
            index++;

        ret += index;
    NODE_CASE_OVERFLOW
        ret += it->index;
    NODE_CASE_INT
        const memory &minK = it->minKey;
        const memory &maxK = it->index < internal->branchCount() - 1 ? internal->branch(it->index + 1).minKey : it->maxKey;

        int delta = pendingRankDelta(node, minK, maxK);

        for (keycount_t i = 0; i < it->index; i++)
        {
            ret += internal->branch(i).itemCount;
        }

        ret += delta;

    NODE_CASE_END
    }

    return ret;
}

int query_tree_impl::pendingRankDelta(const node_ptr &node, const memory &minKey, const memory &maxKey)
{
    // Doesn't STRICTLY speaking need a node_ptr argument, but it makes the tree traversal a bit
    // faster to not have to start at the root.

    int delta = 0;

    editmap_t::iterator start = minKey.empty() ? m_edits.begin() : m_edits.lower_bound(minKey);
    editmap_t::iterator end = maxKey.empty() ? m_edits.end() : m_edits.lower_bound(maxKey);

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
                applyPendingChangeRec(node, *it, NULL, NULL);
                kit->second.erase(it);
            }
        }

        // Erase key iff the list is now empty
        if (kit->second.empty())
            m_edits.erase(kit++);
        else
            ++kit;
    }

    return delta;
}

query_iterator_impl_ptr query_tree_impl::begin()
{
    treepath_t rootPath;
    rootPath.push_back(knuckle(root(), memory(), memory()));
    query_iterator_impl_ptr it;
    findRec(rootPath, NULL, &it);
    return it;
}

void query_tree_impl::applyPendingChanges(const memory &minKey, const memory &maxKey, int *delta, pairlist_t::iterator *leafIter)
{
    editmap_t::iterator start = minKey.empty() ? m_edits.begin() : m_edits.lower_bound(minKey);
    editmap_t::iterator end   = maxKey.empty() ? m_edits.end() : m_edits.lower_bound(maxKey);

    for (editmap_t::iterator kit = start; kit != end; /* increment as part of erase */)
    {
        for (editlist_t::iterator it = kit->second.begin(); it != kit->second.end(); ++it)
            applyPendingChangeRec(root(), *it, delta, leafIter);
        m_edits.erase(kit++); // This is the idiomatic way to iterate+erase
    }
}

void query_tree_impl::applyPendingChangeRec(const node_ptr &node, const pending_edit &edit, int *delta, pairlist_t::iterator *leafIter)
{
NODE_CASE_LEAF
    // We only need to apply changes to leaf nodes. No need to split into overflow blocks since
    // we're never going to write this back.
    bool erased;
    switch (edit.edit)
    {
        case INSERT:
            {
                leaf->insert(kv_pair(edit.key, edit.value));
                if (delta) (*delta)++;
            }
            break;
        case UPSERT:
            {
                pairlist_t::iterator it = leaf->pairs.find(edit.key);
                if (it != leaf->pairs.end())
                    leaf->update_value(it, edit.value);
                else
                {
                    leaf->insert(kv_pair(edit.key, edit.value));
                    if (delta) (*delta)++;
                }
            }
            break;
        case REMOVE_KEY:
            erased = removeFromLeaf(leaf, edit.key, NULL, leafIter);
            if (erased && delta) (*delta)--;
            break;
        case REMOVE_KV:
            erased = removeFromLeaf(leaf, edit.key, &edit.value, leafIter);
            if (erased && delta) (*delta)--;
            break;
        default:
            assert(false);
            throw std::runtime_error("Unrecognized queued command.");
    };

NODE_CASE_INT
    // Drill down to leaf, only to update counts afterwards
    keycount_t i = FindInternalKey(internal, edit.key, m_fns);
    node_ptr c = child(internal->branch(i));
    applyPendingChangeRec(c, edit, delta, leafIter);
    internal->branch(i).itemCount = c->itemCount();

NODE_CASE_END
}


}
