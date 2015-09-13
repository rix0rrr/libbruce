#include "edit_tree_impl.h"

#include "nodes.h"
#include "serializing.h"
#include <cassert>

#include <boost/lexical_cast.hpp>
#include <boost/foreach.hpp>

// Macro because we don't have C++11
#define to_string boost::lexical_cast<std::string>
#define IMPLIES(a, b) ((!a) || b)

namespace bruce {

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

edit_tree_impl::edit_tree_impl(be::be &be, maybe_nodeid rootID, const tree_functions &fns)
    : tree_impl(be, rootID, fns), m_frozen(false), m_newIDsRequired(0)
{
}

void edit_tree_impl::insert(const memory &key, const memory &value)
{
    checkNotFrozen();
    validateKVSize(key, value);

    const node_ptr &r = root();
    splitresult_t rootSplit = insertRec(r, key, value);

    if (!rootSplit.didSplit)
    {
        m_root = rootSplit.left;
    }
    else
    {
        // Replace root with a new internal node
        internalnode_ptr newRoot = boost::make_shared<InternalNode>();
        newRoot->insert(0, node_branch(memory(), rootSplit.left, rootSplit.left->itemCount()));
        newRoot->insert(1, node_branch(rootSplit.splitKey, rootSplit.right, rootSplit.right->itemCount()));
        m_root = newRoot;
    }
}

void edit_tree_impl::validateKVSize(const memory &key, const memory &value)
{
    uint32_t maxSize = m_be.maxBlockSize();
    if (key.size() + value.size() > maxSize)
        throw std::runtime_error("Key/value too large to insert, max size: " + to_string(maxSize));
}

splitresult_t edit_tree_impl::insertRec(const node_ptr &node, const memory &key, const memory &value)
{
NODE_CASE_LEAF
        keycount_t i = FindLeafKey(leaf, key, m_fns);

        if (i == leaf->pairs.size() && !leaf->overflow.empty())
        {
            // There is an overflow block already. Insert in there.
            splitresult_t split = insertRec(overflowNode(leaf->overflow), key, value);
            leaf->overflow.count = split.left->itemCount();
            return splitresult_t(leaf);
        }

        leaf->insert(i, kv_pair(key, value));

        return maybeSplitLeaf(leaf);
NODE_CASE_OVERFLOW
        // Add at the end of the overflow block chain
        overflow->append(value);
        pushDownOverflowNodeSize(overflow);
        return splitresult_t(overflow);

NODE_CASE_INT
        keycount_t i = FindInternalKey(internal, key, m_fns);
        splitresult_t childSplit = insertRec(child(internal->branch(i)), key, value);
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

memory edit_tree_impl::pullFromOverflow(const node_ptr &node)
{
    overflownode_ptr overflow = boost::static_pointer_cast<OverflowNode>(node);

    if (overflow->next.empty())
    {
        memory ret = overflow->values.back();
        overflow->erase(overflow->valueCount() - 1);
        return ret;
    }

    memory ret = pullFromOverflow(overflowNode(overflow->next));
    overflow->setNext(overflow->next.node);
    return ret;
}

splitresult_t edit_tree_impl::maybeSplitLeaf(const leafnode_ptr &leaf)
{
    LeafNodeSize size(leaf, m_be.maxBlockSize());
    if (!size.shouldSplit())
        return splitresult_t(leaf);

    // Child needs to split
    leafnode_ptr left = boost::make_shared<LeafNode>(
            leaf->at(0),
            leaf->at(size.overflowStart()));
    overflownode_ptr overflow = boost::make_shared<OverflowNode>(
            leaf->at(size.overflowStart()),
            leaf->at(size.splitIndex()));
    leafnode_ptr right = boost::make_shared<LeafNode>(
            leaf->at(size.splitIndex()),
            leaf->pairs.end());

    // It should not be possible that the original leaf had an overflow
    // block and we're NOT producing an actual leaf split right now. Where
    // would we put the original overflow block if not on the right node?
    assert(IMPLIES(!leaf->overflow.empty(), right));

    if (overflow->itemCount())
    {
        left->setOverflow(overflow);
        pushDownOverflowNodeSize(overflow);
    }

    if (right->itemCount())
    {
        // Copy the overflow from the current node
        right->overflow = leaf->overflow;
        return splitresult_t(left, right->minKey(), right);
    }
    else
        return splitresult_t(left);
}

splitresult_t edit_tree_impl::maybeSplitInternal(const internalnode_ptr &internal)
{
    InternalNodeSize size(internal, m_be.maxBlockSize());

    // Maybe split this node
    if (!size.shouldSplit())
        return splitresult_t(internal);

    keycount_t j = size.splitIndex();
    internalnode_ptr left = boost::make_shared<InternalNode>(internal->branches.begin(), internal->branches.begin() + j);
    internalnode_ptr right = boost::make_shared<InternalNode>(internal->branches.begin() + j, internal->branches.end());

    return splitresult_t(left, right->minKey(), right);
}

void edit_tree_impl::updateBranch(const internalnode_ptr &internal, keycount_t i, const splitresult_t &split)
{
    // Child didn't split
    if (!split.didSplit)
    {
        internal->branches[i].child = split.left;
        internal->branches[i].itemCount = split.left->itemCount();
        return;
    }

    // Child did split, replace single branch with two branches (in effect,
    // treat current as left and add one to the right).
    internal->branches[i].child = split.left;
    internal->branches[i].itemCount = split.left->itemCount();
    node_branch rightBranch(split.splitKey, split.right, split.right->itemCount());
    internal->insert(i + 1, rightBranch);
}

void edit_tree_impl::checkNotFrozen()
{
    if (m_frozen)
        throw std::runtime_error("Can't mutate tree anymore; already flushed");
}

bool edit_tree_impl::remove(const memory &key)
{
    checkNotFrozen();

    node_ptr r = root();
    itemcount_t count = r->itemCount();
    return removeRec(r, key, NULL).left->itemCount() != count;
}

bool edit_tree_impl::remove(const memory &key, const memory &value)
{
    checkNotFrozen();

    node_ptr r = root();
    itemcount_t count = r->itemCount();
    return removeRec(r, key, &value).left->itemCount() != count;
}

splitresult_t edit_tree_impl::removeRec(const node_ptr &node, const memory &key, const memory *value)
{
    // We're supposed to be joining nodes together if they're below half-max,
    // but I'm skipping out on that right now.
    //
    // However, when shifting values from an overflow node, we may need to split
    // our leaf.
NODE_CASE_LEAF
    index_range keyrange = findLeafRange(leaf, key);

    // Regular old remove from this block
    keycount_t eraseLocation = leaf->pairs.size();
    for (keycount_t i = keyrange.start; i < keyrange.end; i++)
    {
        if (!value || m_fns.valueCompare(leaf->pair(i).value, *value) == 0)
        {
            eraseLocation = i;
            break;
        }
    }

    if (eraseLocation < leaf->pairs.size())
    {
        // Did erase
        leaf->pairs.erase(leaf->at(eraseLocation));

        // If we removed the final position, pull back from the overflow block
        // and potentially split.
        if (eraseLocation == leaf->pairs.size() && !leaf->overflow.empty())
        {
            memory ret = pullFromOverflow(leaf->overflow.node);
            leaf->pairs.push_back(kv_pair(key, ret));
            leaf->setOverflow(leaf->overflow.node);

            return maybeSplitLeaf(leaf);
        }
    }
    else if (!leaf->overflow.empty() && key == leaf->pairs.back().key)
    {
        // Did not erase from this leaf but key matches overflow key, recurse
        splitresult_t overflowSplit = removeRec(overflowNode(leaf->overflow), key, value);
        leaf->setOverflow(overflowSplit.left);
    }

    return splitresult_t(leaf);

NODE_CASE_OVERFLOW
    // Try to remove from this block
    bool erased = false;
    for (valuelist_t::const_iterator it = overflow->values.begin(); it != overflow->values.end(); ++it)
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
        splitresult_t overflowSplit = removeRec(overflowNode(overflow->next), key, value);
        overflow->setNext(overflowSplit.left);
    }

    // If this block is now empty, pull a value from the next one
    if (!overflow->itemCount() && !overflow->next.empty())
    {
        memory value = pullFromOverflow(overflowNode(overflow->next));
        overflow->append(value);
    }

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

    m_newIDsRequired = 1; // For the root
    collectNewIDsRec(root());

    m_be.newIdentifiers(m_newIDsRequired).swap(m_newIDs); // Swaptimization

    m_putBlocks.reserve(m_newIDsRequired);
    m_newIDsRequired = 1; // For the root
    collectBlocksToPutRec(root(), m_newIDs[0]);

    m_be.put_all(m_putBlocks);

    return collectMutation();
}

mutation edit_tree_impl::collectMutation()
{
    mutation ret(m_newIDs[0]);
    bool failed = false;

    BOOST_FOREACH(const be::putblock_t &put, m_putBlocks)
    {
        if (put.success)
            ret.addCreated(put.id);
        else
            failed = true;
    }
    if (failed)
        ret.fail("Failed to write some blocks to the block engine");

    BOOST_FOREACH(nodeid_t id, m_loadedIDs)
        ret.addObsolete(id);

    return ret;
}

/**
 * Traverse the tree, and record that we need a new ID for every node that was loaded
 *
 * Every loaded node got loaded because we changed it, so it needs to be
 * written to a new page. Every branch with a child pointer gets its nodeID
 * replaced with an index into the m_nodeIDs array. After provisioning the new
 * IDs, that array will have the actual IDs for every page.
 */
void edit_tree_impl::collectNewIDsRec(node_ptr &node)
{
    // FIXME: Check if we actually did any changes. Otherwise we have nothing to write.

NODE_CASE_LEAF
    if (leaf->overflow.node)
    {
        leaf->overflow.nodeID = m_newIDsRequired++;
        collectNewIDsRec(leaf->overflow.node);
    }

NODE_CASE_OVERFLOW
    if (overflow->next.node)
    {
        overflow->next.nodeID = m_newIDsRequired++;
        collectNewIDsRec(overflow->next.node);
    }

NODE_CASE_INT
    BOOST_FOREACH(node_branch &b, internal->branches)
    {
        if (b.child)
        {
            b.nodeID = m_newIDsRequired++;
            collectNewIDsRec(b.child);
        }
    }

NODE_CASE_END
}

void edit_tree_impl::collectBlocksToPutRec(node_ptr &node, nodeid_t id)
{
NODE_CASE_LEAF
    if (leaf->overflow.node)
    {
        leaf->overflow.nodeID = m_newIDs[leaf->overflow.nodeID];
        collectBlocksToPutRec(leaf->overflow.node, leaf->overflow.nodeID);
    }

NODE_CASE_OVERFLOW
    if (overflow->next.node)
    {
        overflow->next.nodeID = m_newIDs[overflow->next.nodeID];
        collectBlocksToPutRec(overflow->next.node, overflow->next.nodeID);
    }

NODE_CASE_INT
    BOOST_FOREACH(node_branch &b, internal->branches)
    {
        if (b.child)
        {
            // In this case, nodeID is an index into the array of IDs
            b.nodeID = m_newIDs[b.nodeID];
            collectBlocksToPutRec(b.child, b.nodeID);
        }
    }

NODE_CASE_END

    // Serialize this node
    m_putBlocks.push_back(be::putblock_t(id, SerializeNode(node)));
}


}
