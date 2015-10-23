#include "edit_tree_impl.h"

#include <cassert>

#include <boost/lexical_cast.hpp>

#include "nodes.h"
#include "leaf_node.h"
#include "internal_node.h"
#include "overflow_node.h"
#include "serializing.h"
#include "helpers.h"

namespace libbruce {

//----------------------------------------------------------------------

edit_tree_impl::edit_tree_impl(be::be &be, maybe_nodeid rootID, mempool &mempool, const tree_functions &fns)
    : tree_impl(be, rootID, mempool, fns), m_frozen(false)
{
}

void edit_tree_impl::insert(const memslice &key, const memslice &value, bool upsert)
{
    checkNotFrozen();
    validateKVSize(key, value);

    if (upsert)
        apply(root(), pending_edit(UPSERT, key, value, false), SHALLOW);
    else
        apply(root(), pending_edit(INSERT, key, value, true), SHALLOW);
}

void edit_tree_impl::remove(const memslice &key)
{
    checkNotFrozen();
    apply(root(), pending_edit(REMOVE_KEY, key, memslice(), false), SHALLOW);
}

void edit_tree_impl::remove(const memslice &key, const memslice &value)
{
    checkNotFrozen();
    apply(root(), pending_edit(REMOVE_KV, key, value, false), SHALLOW);
}

void edit_tree_impl::checkNotFrozen()
{
    if (m_frozen)
        throw std::runtime_error("Can't mutate tree anymore; already flushed");
}

void edit_tree_impl::validateKVSize(const memslice &key, const memslice &value)
{
    uint32_t maxSize = m_be.maxBlockSize();
    if (key.size() + value.size() > maxSize)
        throw std::runtime_error("Key/value too large to insert, max size: " + to_string(maxSize));
}

mutation edit_tree_impl::flush()
{
    m_frozen = true;

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

splitresult_t edit_tree_impl::flushAndSplitRec(node_ptr &node)
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
    for (branchlist_t::iterator it = internal->branches.begin(); it != internal->branches.end(); ++it)
    {
        if (it->child)
        {
            int i = it - internal->branches.begin();
            updateBranch(internal, i, flushAndSplitRec(it->child));
        }
    }

    // Then check for splitting
    return maybeSplitInternal(internal);
NODE_CASE_END
}

splitresult_t edit_tree_impl::maybeSplitLeaf(const leafnode_ptr &leaf)
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

void edit_tree_impl::pushDownOverflowNodeSize(const overflownode_ptr &overflow)
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
 * If the queued up edits have exceeded the maximum allowed block size, apply them to the blocks below.
 */
void edit_tree_impl::maybeApplyEdits(const internalnode_ptr &internal)
{
    InternalNodeSize size(internal, m_be.maxBlockSize(), m_be.editQueueSize());
    if (!size.shouldApplyEditQueue())
        return;

    loadBlocksToEdit(internal);

    // Now apply edits to leaves below and clear
    for (editlist_t::const_iterator it = internal->editQueue.begin(); it != internal->editQueue.end(); ++it)
    {
        keycount_t i = FindInternalKey(internal, it->key, m_fns);
        apply(internal->branches[i].child, *it, SHALLOW);
    }
    internal->editQueue.clear();
}

void edit_tree_impl::loadBlocksToEdit(const internalnode_ptr &internal)
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

be::blockidlist_t edit_tree_impl::findBlocksToFetch(const internalnode_ptr &internal)
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

splitresult_t edit_tree_impl::maybeSplitInternal(const internalnode_ptr &internal)
{
    InternalNodeSize size(internal, m_be.maxBlockSize(), m_be.editQueueSize());

    // Maybe split this node
    if (!size.shouldSplit())
        return splitresult_t(internal);

    keycount_t j = size.splitIndex();
    internalnode_ptr left = boost::make_shared<InternalNode>(internal->branches.begin(), internal->branches.begin() + j);
    internalnode_ptr right = boost::make_shared<InternalNode>(internal->branches.begin() + j, internal->branches.end());

    // Might be that the right node is too big, so split it again, then adjust
    // the keys and prepend the left branch.
    splitresult_t split = maybeSplitInternal(right);
    split.left().minKey = right->minKey();
    split.branches.insert(split.branches.begin(), node_branch(memslice(), left));
    return split;
}

void edit_tree_impl::updateBranch(const internalnode_ptr &internal, keycount_t i, const splitresult_t &split)
{
    // For the first one, update but don't change the minKey
    internal->branches[i].child = split.left().child;
    internal->branches[i].itemCount = split.left().itemCount;

    if (!split.didSplit())
    {
        // Child didn't split, so maybe it got reduced and is now empty
        if (!internal->branches[i].itemCount)
            internal->erase(i);
    }
    else
    {
        // Insert the rest
        for (int j = 1; j < split.branches.size(); j++)
            internal->insert(i + 1 + j, split.branches[j]);
    }
}

nodeid_t edit_tree_impl::collectBlocksRec(node_ptr &node)
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

mutation edit_tree_impl::collectMutation()
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


}
