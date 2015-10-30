#include "tree_impl.h"

#include "serializing.h"
#include "leaf_node.h"
#include "internal_node.h"
#include "overflow_node.h"
#include "helpers.h"

namespace libbruce {

tree_impl::tree_impl(be::be &be, maybe_nodeid rootID, mempool &mempool, const tree_functions &fns)
    : m_be(be), m_rootID(rootID), m_mempool(mempool), m_fns(fns)
{
}

void tree_impl::insert(const memslice &key, const memslice &value)
{
    validateKVSize(key, value);

    apply(root(), pending_edit(INSERT, key, value, true), SHALLOW);
}

void tree_impl::upsert(const memslice &key, const memslice &value, bool guaranteed)
{
    validateKVSize(key, value);

    apply(root(), pending_edit(UPSERT, key, value, guaranteed), SHALLOW);
}

void tree_impl::remove(const memslice &key, bool guaranteed)
{
    apply(root(), pending_edit(REMOVE_KEY, key, memslice(), guaranteed), SHALLOW);
}

void tree_impl::remove(const memslice &key, const memslice &value, bool guaranteed)
{
    apply(root(), pending_edit(REMOVE_KV, key, value, guaranteed), SHALLOW);
}

//----------------------------------------------------------------------
//  Loading
//

node_ptr &tree_impl::root()
{
    if (!m_root)
    {
        if (m_rootID)
            m_root = load(*m_rootID);
        else
            // No existing tree, create a new leaf node
            m_root = boost::make_shared<LeafNode>(m_fns);
    }

    return m_root;
}

const node_ptr &tree_impl::child(node_branch &branch)
{
    if (!branch.child) branch.child = load(branch.nodeID);
    return branch.child;
}

const node_ptr &tree_impl::overflowNode(overflow_t &overflow)
{
    assert(!overflow.empty());
    if (!overflow.node) overflow.node = load(overflow.nodeID);
    return overflow.node;
}

node_ptr tree_impl::load(nodeid_t id)
{
    m_loadedIDs.push_back(id);
    return deserialize(m_be.get(id));
}

node_ptr tree_impl::deserialize(const mempage &mem)
{
    m_mempool.retain(mem);
    return ParseNode(mem, m_fns);
}

//----------------------------------------------------------------------
//  Editing
//

void tree_impl::apply(const pending_edit &edit, Depth depth)
{
    apply(root(), edit, depth);
}

void tree_impl::apply(const node_ptr &node, const pending_edit &edit, Depth depth)
{
NODE_CASE_LEAF
    switch (edit.edit)
    {
        case INSERT:
            leafInsert(leaf, edit.key, edit.value, false, NULL);
            break;
        case UPSERT:
            leafInsert(leaf, edit.key, edit.value, true, NULL);
            break;
        case REMOVE_KEY:
            leafRemove(leaf, edit.key, NULL, NULL);
            break;
        case REMOVE_KV:
            leafRemove(leaf, edit.key, &edit.value, NULL);
            break;
    }

NODE_CASE_OVERFLOW
    // Shouldn't ever be called, editing the overflow node is taken
    // care of by the LEAF case.
    assert(false);

NODE_CASE_INT
    if (depth == DEEP)
    {
        keycount_t i = FindInternalKey(internal, edit.key, m_fns);
        apply(child(internal->branches[i]), edit, depth);
        internal->branches[i].itemCount = internal->branches[i].child->itemCount();
    }
    else
    {
        // Add in sorted location, but at the end of the same keys
        editlist_t::iterator it = std::upper_bound(internal->editQueue.begin(),
                                                internal->editQueue.end(),
                                                edit.key,
                                                EditOrder(m_fns));
        internal->editQueue.insert(it, edit);
    }

NODE_CASE_END
}

void tree_impl::leafInsert(const leafnode_ptr &leaf, const memslice &key, const memslice &value, bool upsert, uint32_t *delta)
{
    pairlist_t::iterator it = leaf->find(key);
    if (upsert && it != leaf->pairs.end())
    {
        // Update
        leaf->update_value(it, value);
    }
    else
    {
        // Insert

        // If we found PAST the final key and there is an overflow block, we need to pull the entire
        // overflow block in and insert it after that.
        if (it == leaf->pairs.end() && !leaf->overflow.empty())
        {
            memslice o_key = leaf->pairs.rbegin()->first;
            while (!leaf->overflow.empty())
            {
                memslice o_value = overflowPull(leaf->overflow);
                leaf->insert(kv_pair(o_key, o_value));
            }
            leaf->insert(kv_pair(key, value));
            if (delta) (*delta)++;
        }
        // If we found the final key, insert into the overflow block
        else if (it == (--leaf->pairs.end()) && !leaf->overflow.empty())
        {
            // There is an overflow block already. Insert in there.
            overflowInsert(leaf->overflow, value, delta);
        }
        else
        {
            leaf->insert(kv_pair(key, value));
            if (delta) (*delta)++;
        }
    }
}

void tree_impl::leafRemove(const leafnode_ptr &leaf, const memslice &key, const memslice *value, uint32_t *delta)
{
    if (leaf->pairs.empty()) return;

    pairlist_t::iterator begin, end;
    leaf->findRange(key, &begin, &end);

    pairlist_t::iterator eraseLocation = leaf->pairs.end();

    // Regular old remove from this block
    for (pairlist_t::iterator it = begin; it != end; ++it)
    {
        if (!value || m_fns.valueCompare(it->second, *value) == 0)
        {
            eraseLocation = it;
            break;
        }
    }

    if (eraseLocation != leaf->pairs.end())
    {
        // Did erase in this block
        eraseLocation = leaf->erase(eraseLocation);

        // If we removed the final position, pull back from the overflow block.
        if (eraseLocation == leaf->pairs.end() && !leaf->overflow.empty())
        {
            memslice ret = overflowPull(leaf->overflow);
            leaf->insert(kv_pair(key, ret));
        }

        if (delta) (*delta)--;
    }
    // If we did not erase here, but the key matches the last key, search in the overflow block
    else if (!leaf->overflow.empty() && key == leaf->pairs.rbegin()->first)
    {
        // Did not erase from this leaf but key matches overflow key, recurse
        return overflowRemove(leaf->overflow, value, delta);
    }
}

void tree_impl::overflowInsert(overflow_t &overflow_rec, const memslice &value, uint32_t *delta)
{
    overflownode_ptr overflow = boost::static_pointer_cast<OverflowNode>(overflowNode(overflow_rec));
    overflow->append(value);
    if (delta) (*delta)++;
    overflow_rec.count = overflow->itemCount();
}

void tree_impl::overflowRemove(overflow_t &overflow_rec, const memslice *value, uint32_t *delta)
{
    overflownode_ptr overflow = boost::static_pointer_cast<OverflowNode>(overflowNode(overflow_rec));

    // Try to remove from this block
    bool erased = false;
    for (valuelist_t::iterator it = overflow->values.begin(); it != overflow->values.end(); ++it)
    {
        if (!value || m_fns.valueCompare(*it, *value) == 0)
        {
            overflow->values.erase(it);
            erased = true;
        }
    }

    // Try to remove from the next block
    if (!erased && !overflow->next.empty())
    {
        overflowRemove(overflow->next, value, delta);
    }

    // If this block is now empty, pull a value from the next one
    if (!overflow->itemCount() && !overflow->next.empty())
    {
        memslice value = overflowPull(overflow->next);
        overflow->append(value);
    }

    overflow_rec.count = overflow->values.size() + overflow->next.count;

    if (erased && delta) (*delta)--;
}

memslice tree_impl::overflowPull(overflow_t &overflow_rec)
{
    overflownode_ptr overflow = boost::static_pointer_cast<OverflowNode>(overflowNode(overflow_rec));

    if (overflow->next.empty())
    {
        memslice ret = overflow->values.back();
        overflow->erase(overflow->valueCount() - 1);
        overflow_rec.count = overflow->values.size();
        return ret;
    }

    memslice ret = overflowPull(overflow->next);
    overflow_rec.count = overflow->values.size() + overflow->next.count;
    return ret;
}

void tree_impl::applyEditsToBranch(const internalnode_ptr &internal, const keycount_t &i)
{
    editlist_t::iterator editBegin = internal->editQueue.begin();
    editlist_t::iterator editEnd = internal->editQueue.end();

    if (i > 0) editBegin = std::lower_bound(internal->editQueue.begin(), internal->editQueue.end(), internal->branches[i].minKey, EditOrder(m_fns));
    if (i < internal->branches.size() - 1) editEnd = std::lower_bound(internal->editQueue.begin(), internal->editQueue.end(), internal->branches[i+1].minKey, EditOrder(m_fns));

    if (editBegin == editEnd) return; // Nothing to apply

    assert(internal->branches[i].child);

    if (internal->branches[i].child->nodeType() == TYPE_LEAF)
        boost::static_pointer_cast<LeafNode>(internal->branches[i].child)->applyAll(editBegin, editEnd);
    else
    {
        for (editlist_t::const_iterator it = editBegin; it != editEnd; ++it)
        {
            apply(internal->branches[i].child, *it, SHALLOW);
        }
    }
}

void tree_impl::validateKVSize(const memslice &key, const memslice &value)
{
    uint32_t maxSize = m_be.maxBlockSize();
    if (key.size() + value.size() > maxSize)
        throw std::runtime_error("Key/value too large to insert, max size: " +
                                 boost::lexical_cast<std::string>(maxSize));
}

mutation tree_impl::write()
{
    // Root not loaded == no changes
    if (!m_root)
        return mutation(m_rootID);

    // FIXME: Check if we actually did any changes. Otherwise we have nothing to write.

    splitresult_t rootSplit = flushAndSplitRec(root());

    // Try splitting the new root node a max number of times.
    // The block size may be too small to contain the split key + node
    // references. It's hard to determine statically, so we'll just try to
    // converge for a number of rounds and then give up.
    int tries = 4;
    while (rootSplit.didSplit() && tries--)
    {
        // Replace root with a new internal node
        internalnode_ptr newRoot = boost::make_shared<InternalNode>();
        newRoot->branches = rootSplit.branches;

        rootSplit = maybeSplitInternal(newRoot);
    }

    if (rootSplit.didSplit())
        throw std::runtime_error("Block size is too small for these keys");

    m_root = rootSplit.left().child;

    m_rootID = collectBlocksRec(root());

    m_be.put_all(m_putBlocks);

    return collectMutation();
}

splitresult_t tree_impl::flushAndSplitRec(node_ptr &node)
{
NODE_CASE_LEAF
    // Make sure that we recurse into the overflow nodes
    // (This will never produce a split)
    if (!leaf->overflow.empty() && leaf->overflow.node)
        flushAndSplitRec(leaf->overflow.node);

    // Finally split this node if necessary
    return maybeSplitLeaf(leaf);

NODE_CASE_OVERFLOW
    // Make sure that the overflow blocks are not too big
    pushDownOverflowNodeSize(overflow);
    return splitresult_t(overflow);

NODE_CASE_INT
    maybeApplyEdits(internal);

    // Then flush children
    for (branchlist_t::iterator it = internal->branches.begin(); it != internal->branches.end(); )
    {
        if (it->child)
        {
            int i = it - internal->branches.begin();
            it = updateBranch(internal, it, flushAndSplitRec(it->child));
        }
        else
            ++it;
    }

    // Then check for splitting
    return maybeSplitInternal(internal);
NODE_CASE_END
}

splitresult_t tree_impl::maybeSplitLeaf(const leafnode_ptr &leaf)
{
    LeafNodeSize size(leaf, m_be.maxBlockSize());
    if (!size.shouldSplit())
        return splitresult_t(leaf);

    // Child needs to split
    leafnode_ptr left = boost::make_shared<LeafNode>(
            leaf->pairs.begin(),
            size.overflowStart(),
            m_fns);
    overflownode_ptr overflow = boost::make_shared<OverflowNode>(
            size.overflowStart(),
            size.splitStart());
    leafnode_ptr right = boost::make_shared<LeafNode>(
            size.splitStart(),
            leaf->pairs.end(),
            m_fns);

    // It should not be possible that the original leaf had an overflow
    // block and we're NOT producing an actual leaf split right now. Where
    // would we put the original overflow block if not on the right node?
    assert(IMPLIES(!leaf->overflow.empty(), right->itemCount()));

    // The new overflow goes in the middle
    if (overflow->itemCount())
    {
        left->setOverflow(overflow);
        pushDownOverflowNodeSize(overflow);
    }

    if (right->itemCount())
    {
        // Any old overflow goes onto the right
        right->overflow = leaf->overflow;

        // At this point, it might be the case that the right node is too large,
        // so check for splitting it again, and then simply adjust the keys
        // on the return object and prepend the left branch.
        splitresult_t split = maybeSplitLeaf(right);
        split.left().minKey = right->minKey();
        split.branches.insert(split.branches.begin(), node_branch(memslice(), left));
        return split;
    }
    else
        return splitresult_t(left);
}

void tree_impl::pushDownOverflowNodeSize(const overflownode_ptr &overflow)
{
    OverflowNodeSize size(overflow, m_be.maxBlockSize());
    if (!size.shouldSplit())
        return;

    // Move values exceeding size to the next block
    if (overflow->next.empty())
        overflow->next.node = boost::make_shared<OverflowNode>();

    overflownode_ptr next = boost::static_pointer_cast<OverflowNode>(overflow->next.node);

    // Push everything exceeding the size to the next block
    for (unsigned i = size.splitIndex(); i < overflow->values.size(); i++)
        next->values.push_back(overflow->values[i]);
    overflow->values.erase(overflow->at(size.splitIndex()), overflow->values.end());

    pushDownOverflowNodeSize(next);
    overflow->next.count = next->itemCount();
}

/**
 * Maybe apply edits
 *
 * - If the queued up edits have exceeded the maximum allowed block size,
 *   apply all edits.
 * - Apply edits that are not guaranteed, or are followed by
 *   edits for the same key that are nonguaranteed.
 */
void tree_impl::maybeApplyEdits(const internalnode_ptr &internal)
{
    InternalNodeSize size(internal, m_be.maxBlockSize(), m_be.editQueueSize());
    if (!size.shouldApplyEditQueue())
        return;

    loadBlocksToEdit(internal);

    // Now apply edits to leaves below and clear
    for (keycount_t i = 0; i < internal->branchCount(); i++)
    {
        applyEditsToBranch(internal, i);
    }

    internal->editQueue.clear();
}

void tree_impl::loadBlocksToEdit(const internalnode_ptr &internal)
{
    be::blockidlist_t ids = findBlocksToFetch(internal);

    be::getblockresult_t pages = m_be.get_all(ids);

    // Deserialize blocks and assign to branches
    for (branchlist_t::iterator it = internal->branches.begin(); it != internal->branches.end(); ++it)
    {
        be::getblockresult_t::const_iterator found = pages.find(it->nodeID);
        if (found != pages.end())
        {
            m_loadedIDs.push_back(found->first);
            it->child = deserialize(found->second);
        }
    }
}

be::blockidlist_t tree_impl::findBlocksToFetch(const internalnode_ptr &internal)
{
    std::set<nodeid_t> ids;

    for (editlist_t::const_iterator it = internal->editQueue.begin(); it != internal->editQueue.end(); ++it)
    {
        keycount_t i = FindInternalKey(internal, it->key, m_fns);
        // Only if not loaded yet
        if (!internal->branches[i].child) ids.insert(internal->branches[i].nodeID);
    }

    return be::blockidlist_t(ids.begin(), ids.end());
}

splitresult_t tree_impl::maybeSplitInternal(const internalnode_ptr &internal)
{
    InternalNodeSize size(internal, m_be.maxBlockSize(), m_be.editQueueSize());

    // Maybe split this node
    if (!size.shouldSplit())
        return splitresult_t(internal);

    keycount_t j = size.splitIndex();
    internalnode_ptr left = boost::make_shared<InternalNode>(internal->branches.begin(), internal->branches.begin() + j);
    internalnode_ptr right = boost::make_shared<InternalNode>(internal->branches.begin() + j, internal->branches.end());

    // Divide the edits over the new internals
    editlist_t::iterator editSplit = std::lower_bound(internal->editQueue.begin(), internal->editQueue.end(), right->minKey(), EditOrder(m_fns));
    left->editQueue.insert(left->editQueue.end(), internal->editQueue.begin(), editSplit);
    right->editQueue.insert(right->editQueue.end(), editSplit, internal->editQueue.end());

    // Might be that the right node is too big, so split it again, then adjust
    // the keys and prepend the left branch.
    splitresult_t split = maybeSplitInternal(right);
    split.left().minKey = right->minKey();
    split.branches.insert(split.branches.begin(), node_branch(memslice(), left));
    return split;
}

branchlist_t::iterator tree_impl::updateBranch(const internalnode_ptr &internal, branchlist_t::iterator it, const splitresult_t &split)
{
    // For the first one, update but don't change the minKey
    it->child = split.left().child;
    it->itemCount = split.left().itemCount;

    if (!split.didSplit())
    {
        // Child didn't split, so maybe it got reduced and is now empty
        if (!it->itemCount)
            return internal->branches.erase(it);
    }

    // Insert the rest (saving the iterator)
    // We are guaranteed that the inserted branches don't need to be split anymore, so we can
    // just skip those on the return iterator.
    ++it;
    int index = it - internal->branches.begin();
    internal->branches.insert(it, ++split.branches.begin(), split.branches.end());
    return internal->branches.begin() + index + split.branches.size() - 1;
}

nodeid_t tree_impl::collectBlocksRec(node_ptr &node)
{
NODE_CASE_LEAF
    if (!leaf->overflow.empty() && leaf->overflow.node)
        leaf->overflow.nodeID = collectBlocksRec(leaf->overflow.node);

NODE_CASE_OVERFLOW
    if (!overflow->next.empty() && overflow->next.node)
        overflow->next.nodeID = collectBlocksRec(overflow->next.node);

NODE_CASE_INT
    for (branchlist_t::iterator it = internal->branches.begin(); it != internal->branches.end(); ++it)
    {
        if (it->child)
            it->nodeID = collectBlocksRec(it->child);
    }

NODE_CASE_END

    // Serialize this node, request an ID for it, and store it to put later
    mempage serialized = SerializeNode(node);
    nodeid_t id = m_be.id(serialized);
    m_putBlocks.push_back(be::putblock_t(id, serialized));
    return id;
}

mutation tree_impl::collectMutation()
{
    mutation ret(m_rootID);
    bool failed = false;

    for (be::putblocklist_t::const_iterator it = m_putBlocks.begin(); it != m_putBlocks.end(); ++it)
    {
        if (it->success)
            ret.addCreated(it->id);
        else
            failed = true;
    }
    if (failed)
        ret.fail("Failed to write some blocks to the block engine");

    for (std::vector<nodeid_t>::const_iterator it = m_loadedIDs.begin(); it != m_loadedIDs.end(); ++it)
        ret.addObsolete(*it);

    return ret;
}

bool tree_impl::get(const memslice &key, memslice *value)
{
    query_iterator_unsafe it = find(key);
    if (it) *value = it.value();
    return it;
}

query_iterator_impl_ptr tree_impl::find(const memslice &key)
{
    treepath_t rootPath;
    rootPath.push_back(fork(root(), memslice(), memslice()));

    query_iterator_impl_ptr it;
    findRec(rootPath, &key, &it);
    return it;
}

query_iterator_impl_ptr tree_impl::seek(itemcount_t n)
{
    treepath_t rootPath;
    rootPath.push_back(fork(root(), memslice(), memslice()));

    query_iterator_impl_ptr it;
    seekRec(rootPath, n, &it);
    return it;
}

fork tree_impl::travelDown(const fork &top, keycount_t i)
{
    internalnode_ptr internal = boost::static_pointer_cast<InternalNode>(top.node);

    const memslice &minK = internal->branch(i).minKey.size() ? internal->branch(i).minKey : top.minKey;
    const memslice &maxK = i < internal->branchCount() - 1 ? internal->branch(i+1).minKey : top.maxKey;

    node_ptr node = child(internal->branches[i]);
    return fork(node, minK, maxK);
}

void tree_impl::findRec(treepath_t &rootPath, const memslice *key, query_iterator_impl_ptr *iter_ptr)
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

void tree_impl::seekRec(treepath_t &rootPath, itemcount_t n, query_iterator_impl_ptr *iter_ptr)
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

void tree_impl::applyPendingEdits(const internalnode_ptr &internal, fork &frk, node_branch &branch, Depth depth)
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

void tree_impl::findPendingEdits(const internalnode_ptr &internal, fork &frk, editlist_t::iterator *editBegin, editlist_t::iterator *editEnd)
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
bool tree_impl::isGuaranteed(const editlist_t::iterator &begin, const editlist_t::iterator &end)
{
    for (editlist_t::iterator it = begin; it != end; ++it)
        if (!it->guaranteed)
            return false;
    return true;
}

itemcount_t tree_impl::rank(treepath_t &rootPath)
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

int tree_impl::pendingRankDelta(const internalnode_ptr &internal, fork &top, node_branch &branch)
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

query_iterator_impl_ptr tree_impl::begin()
{
    treepath_t rootPath;
    rootPath.push_back(fork(root(), memslice(), memslice()));
    query_iterator_impl_ptr it;
    findRec(rootPath, NULL, &it);
    return it;
}


}
