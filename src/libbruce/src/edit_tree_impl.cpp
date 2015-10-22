#include "edit_tree_impl.h"

#include <cassert>

#include <boost/lexical_cast.hpp>

#include "nodes.h"
#include "serializing.h"
#include "helpers.h"

namespace libbruce {

//----------------------------------------------------------------------

void chop(const overflownode_ptr &overflow, keycount_t i)
{
    overflow->next.node = boost::make_shared<OverflowNode>(
            overflow->at(i),
            overflow->values.end());
    overflow->next.count = overflow->next.node->itemCount();
    overflow->values.erase(overflow->at(i), overflow->values.end());
}

//----------------------------------------------------------------------

edit_tree_impl::edit_tree_impl(be::be &be, maybe_nodeid rootID, mempool &mempool, const tree_functions &fns)
    : tree_impl(be, rootID, mempool, fns), m_frozen(false)
{
}

void edit_tree_impl::insert(const memslice &key, const memslice &value, bool upsert)
{
    checkNotFrozen();
    validateKVSize(key, value);

    const node_ptr &r = root();
    splitresult_t rootSplit = insertRec(r, key, value, upsert);

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
}

void edit_tree_impl::validateKVSize(const memslice &key, const memslice &value)
{
    uint32_t maxSize = m_be.maxBlockSize();
    if (key.size() + value.size() > maxSize)
        throw std::runtime_error("Key/value too large to insert, max size: " + to_string(maxSize));
}

splitresult_t edit_tree_impl::insertRec(const node_ptr &node, const memslice &key, const memslice &value, bool upsert)
{
NODE_CASE_LEAF
    pairlist_t::iterator it = leaf->pairs.find(key);

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
                memslice o_value = pullFromOverflow(leaf->overflow);
                leaf->insert(kv_pair(o_key, o_value));
            }
            leaf->insert(kv_pair(key, value));
        }
        // If we found the final key, insert into the overflow block
        else if (it == (--leaf->pairs.end()) && !leaf->overflow.empty())
        {
            // There is an overflow block already. Insert in there.
            splitresult_t split = insertRec(overflowNode(leaf->overflow), key, value, upsert);
            leaf->overflow.count = split.left().itemCount;
        }
        else
        {
            leaf->insert(kv_pair(key, value));
        }
    }

    return maybeSplitLeaf(leaf);
NODE_CASE_OVERFLOW
    // Add at the end of the overflow block chain
    overflow->append(value);
    pushDownOverflowNodeSize(overflow);
    return splitresult_t(overflow);

NODE_CASE_INT
    keycount_t i = FindInternalKey(internal, key, m_fns);
    splitresult_t childSplit = insertRec(child(internal->branch(i)), key, value, upsert);
    updateBranch(internal, i, childSplit);
    return maybeSplitInternal(internal);

NODE_CASE_END
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

void edit_tree_impl::checkNotFrozen()
{
    if (m_frozen)
        throw std::runtime_error("Can't mutate tree anymore; already flushed");
}

bool edit_tree_impl::remove(const memslice &key)
{
    checkNotFrozen();

    node_ptr r = root();
    itemcount_t count = r->itemCount();
    return removeRec(r, key, NULL).left().itemCount != count;
}

bool edit_tree_impl::remove(const memslice &key, const memslice &value)
{
    checkNotFrozen();

    node_ptr r = root();
    itemcount_t count = r->itemCount();
    return removeRec(r, key, &value).left().itemCount != count;
}

splitresult_t edit_tree_impl::removeRec(const node_ptr &node, const memslice &key, const memslice *value)
{
    // We're supposed to be joining nodes together if they're below half-max,
    // but I'm skipping out on that right now.
    //
    // However, when shifting values from an overflow node, we may need to split
    // our leaf.
NODE_CASE_LEAF
    removeFromLeaf(leaf, key, value);
    return maybeSplitLeaf(leaf);

NODE_CASE_OVERFLOW
    // Won't be called over here
    assert(false);
    return splitresult_t(overflow);

NODE_CASE_INT
    keycount_t i = FindInternalKey(internal, key, m_fns);
    splitresult_t split = removeRec(child(internal->branch(i)), key, value);
    updateBranch(internal, i, split);
    return maybeSplitInternal(internal);

NODE_CASE_END
}

mutation edit_tree_impl::flush()
{
    m_frozen = true;

    // Root not loaded == no changes
    if (!m_root)
        return mutation(m_rootID);

    // FIXME: Check if we actually did any changes. Otherwise we have nothing to write.

    // Replace rootID
    m_rootID = collectBlocksToPutRec(root());
    m_be.put_all(m_putBlocks);

    return collectMutation();
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

nodeid_t edit_tree_impl::collectBlocksToPutRec(node_ptr &node)
{
NODE_CASE_LEAF
    if (!leaf->overflow.empty())
        leaf->overflow.nodeID = collectBlocksToPutRec(leaf->overflow.node);

NODE_CASE_OVERFLOW
    if (!overflow->next.empty())
        overflow->next.nodeID = collectBlocksToPutRec(overflow->next.node);

NODE_CASE_INT
    for (branchlist_t::iterator it = internal->branches.begin(); it != internal->branches.end(); ++it)
    {
        if (it->child)
            it->nodeID = collectBlocksToPutRec(it->child);
    }

NODE_CASE_END

    // Serialize this node, request an ID for it, and store it to put later
    mempage serialized = SerializeNode(node);
    nodeid_t id = m_be.id(serialized);
    m_putBlocks.push_back(be::putblock_t(id, serialized));
    return id;
}


}
