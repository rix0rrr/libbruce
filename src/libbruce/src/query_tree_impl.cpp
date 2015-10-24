#include "query_tree_impl.h"

#include <cassert>
#include <list>

namespace libbruce {

query_tree_impl::query_tree_impl(be::be &be, nodeid_t rootID, mempool &mempool, const tree_functions &fns)
    : tree_impl(be, rootID, mempool, fns)
{
}

void query_tree_impl::queue_insert(const memslice &key, const memslice &value)
{
    apply(root(), pending_edit(INSERT, key, value, true), SHALLOW);
}

void query_tree_impl::queue_upsert(const memslice &key, const memslice &value, bool guaranteed)
{
    apply(root(), pending_edit(UPSERT, key, value, guaranteed), SHALLOW);
}

void query_tree_impl::queue_remove(const memslice &key, bool guaranteed)
{
    apply(root(), pending_edit(REMOVE_KEY, key, memslice(), guaranteed), SHALLOW);
}

void query_tree_impl::queue_remove(const memslice &key, const memslice &value, bool guaranteed)
{
    apply(root(), pending_edit(REMOVE_KV, key, value, guaranteed), SHALLOW);
}

bool query_tree_impl::get(const memslice &key, memslice *value)
{
    query_iterator_unsafe it = find(key);
    if (it) *value = it.value();
    return it;
}

query_iterator_impl_ptr query_tree_impl::find(const memslice &key)
{
    treepath_t rootPath;
    rootPath.push_back(fork(root(), memslice(), memslice()));

    query_iterator_impl_ptr it;
    findRec(rootPath, &key, &it);
    return it;
}

query_iterator_impl_ptr query_tree_impl::seek(itemcount_t n)
{
    treepath_t rootPath;
    rootPath.push_back(fork(root(), memslice(), memslice()));

    query_iterator_impl_ptr it;
    seekRec(rootPath, n, &it);
    return it;
}

fork query_tree_impl::travelDown(const fork &top, keycount_t i)
{
    internalnode_ptr internal = boost::static_pointer_cast<InternalNode>(top.node);

    const memslice &minK = internal->branch(i).minKey.size() ? internal->branch(i).minKey : top.minKey;
    const memslice &maxK = i < internal->branchCount() - 1 ? internal->branch(i+1).minKey : top.maxKey;

    node_ptr node = child(internal->branches[i]);
    return fork(node, minK, maxK);
}

void query_tree_impl::findRec(treepath_t &rootPath, const memslice *key, query_iterator_impl_ptr *iter_ptr)
{
    fork &top = rootPath.back();
    node_ptr node = rootPath.back().node;

NODE_CASE_LEAF
    pairlist_t::iterator begin = leaf->pairs.begin();
    pairlist_t::iterator end = leaf->pairs.end();
    if (key) leaf->findRange(*key, &begin, &end);

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
    fork branch = travelDown(rootPath.back(), top.index);

    applyPendingEdits(internal, branch, internal->branches[top.index], SHALLOW);

    rootPath.push_back(branch);
    findRec(rootPath, key, iter_ptr);

NODE_CASE_END
}

void query_tree_impl::seekRec(treepath_t &rootPath, itemcount_t n, query_iterator_impl_ptr *iter_ptr)
{
    fork &top = rootPath.back();
    node_ptr node = rootPath.back().node;

NODE_CASE_LEAF
    if (n < leaf->pairCount())
    {
        top.leafIter = leaf->get_at(n);
        iter_ptr->reset(new query_iterator_impl(shared_from_this(), rootPath));
        return;
    }

    n -= leaf->pairCount();

    if (!leaf->overflow.empty())
    {
        rootPath.push_back(fork(overflowNode(leaf->overflow), memslice(), memslice()));
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
        rootPath.push_back(fork(overflowNode(overflow->next), memslice(), memslice()));
        seekRec(rootPath, n, iter_ptr);

NODE_CASE_INT
    top.index = 0;

    while (top.index < internal->branchCount())
    {
        // Look for pending changes to apply here
        fork potential = travelDown(rootPath.back(), top.index);
        int delta = pendingRankDelta(internal, potential, internal->branches[top.index]);

        if (n < internal->branch(top.index).itemCount + delta)
        {
            // Found where to descend
            applyPendingEdits(internal, potential, internal->branches[top.index], SHALLOW);
            rootPath.push_back(potential);
            seekRec(rootPath, n, iter_ptr);
            return;
        }
        else
        {
            n -= internal->branch(top.index).itemCount + delta;
            top.index++;
        }
    }

NODE_CASE_END
}

void query_tree_impl::applyPendingEdits(const internalnode_ptr &internal, fork &frk, node_branch &branch, Depth depth)
{
    // If we're applying to a leaf, be sure to save and restore the iterator while modifying
    int index;
    if (frk.nodeType() == TYPE_LEAF) index = frk.leafIndex();

    editlist_t::iterator editBegin, editEnd;
    findPendingEdits(internal, frk, &editBegin, &editEnd);
    for (editlist_t::iterator it = editBegin; it != editEnd; ++it)
        apply(frk.node, *it, depth);

    internal->editQueue.erase(editBegin, editEnd);

    branch.itemCount = frk.node->itemCount();

    if (frk.nodeType() == TYPE_LEAF) frk.setLeafIndex(index);
}

void query_tree_impl::findPendingEdits(const internalnode_ptr &internal, fork &frk, editlist_t::iterator *editBegin, editlist_t::iterator *editEnd)
{
    if (frk.minKey.size() && !internal->editQueue.empty())
        *editBegin = std::lower_bound(internal->editQueue.begin(), internal->editQueue.end(), frk.minKey, EditOrder(m_fns));
    else
        *editBegin = internal->editQueue.begin();

    if (frk.maxKey.size() && !internal->editQueue.empty())
        *editEnd = std::lower_bound(internal->editQueue.begin(), internal->editQueue.end(), frk.maxKey, EditOrder(m_fns));
    else
        *editEnd = internal->editQueue.end();
}

/**
 * Whether all edits in the given range are guaranteed
 */
bool query_tree_impl::isGuaranteed(const editlist_t::iterator &begin, const editlist_t::iterator &end)
{
    for (editlist_t::iterator it = begin; it != end; ++it)
        if (!it->guaranteed)
            return false;
    return true;
}

itemcount_t query_tree_impl::rank(treepath_t &rootPath)
{
    itemcount_t ret = 0;
    for (treepath_t::iterator it = rootPath.begin(); it != rootPath.end(); ++it)
    {
        node_ptr node = it->node;

    NODE_CASE_LEAF
        // This will invalidate the iterator we have into the leaf,
        // so get the index before and readjust afterwards.
        int index = it->leafIndex();
        int delta = 0;
        index += delta;
        it->setLeafIndex(index);

        ret += index;
    NODE_CASE_OVERFLOW
        ret += it->index;
    NODE_CASE_INT
        assert(it->index < internal->branchCount());

        for (keycount_t i = 0; i < it->index; i++)
        {
            // Look for pending changes to apply here
            ret += internal->branches[i].itemCount;
            fork potential = travelDown(*it, i);
            ret += pendingRankDelta(internal, potential, internal->branches[i]);
        }

    NODE_CASE_END
    }

    return ret;
}

int query_tree_impl::pendingRankDelta(const internalnode_ptr &internal, fork &top, node_branch &branch)
{
    // If all pending changes are guaranteed, just calculate the delta. Otherwise apply them deeply
    // and then calculate the delta.

    editlist_t::iterator editBegin, editEnd;
    findPendingEdits(internal, top, &editBegin, &editEnd);

    if (!isGuaranteed(editBegin, editEnd))
    {
        applyPendingEdits(internal, top, branch, DEEP); // deep apply for correct counts
        return 0; // The new count is now in the itemcount
    }

    int delta = 0;
    for (editlist_t::iterator it = editBegin; it != editEnd; ++it)
    {
        delta += it->delta();
    }

    return delta;
}

query_iterator_impl_ptr query_tree_impl::begin()
{
    treepath_t rootPath;
    rootPath.push_back(fork(root(), memslice(), memslice()));
    query_iterator_impl_ptr it;
    findRec(rootPath, NULL, &it);
    return it;
}

}
