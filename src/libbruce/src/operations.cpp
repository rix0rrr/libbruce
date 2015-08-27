#include "operations.h"

#include "nodes.h"
#include "serializing.h"

#include <boost/lexical_cast.hpp>
#include <boost/foreach.hpp>

// Macro because we don't have C++11
#define to_string boost::lexical_cast<std::string>

namespace bruce {

mutable_tree::mutable_tree(be::be &be, maybe_blockid rootID, tree_functions fns)
    : m_be(be), m_rootID(rootID), m_fns(fns), m_frozen(false)
{
}

node_ptr &mutable_tree::root()
{
    if (!m_root)
    {
        if (m_rootID)
            m_root = load(*m_rootID);
        else
            // No existing tree, create a new leaf node
            m_root = boost::make_shared<LeafNode>();
    }

    return m_root;
}

const node_ptr &mutable_tree::child(node_branch &branch)
{
    if (!branch.child) branch.child = load(branch.nodeId);
    return branch.child;
}

node_ptr mutable_tree::load(nodeid_t id)
{
    memory mem = m_be.get(id);
    return ParseNode(mem, m_fns);
}

void mutable_tree::insert(const memory &key, const memory &value)
{
    checkNotFrozen();
    validateKVSize(key, value);

    const node_ptr &r = root();
    splitresult_t rootSplit = insertRec(r, key, value);

    if (rootSplit.didSplit)
    {
        // Replace root with a new internal node
        internalnode_ptr newRoot = boost::make_shared<InternalNode>();
        newRoot->insert(0, node_branch(memory(), 0, rootSplit.leftCount, rootSplit.left));
        newRoot->insert(1, node_branch(rootSplit.splitKey, 0, rootSplit.rightCount, rootSplit.right));
        m_root = newRoot;
    }
}

void mutable_tree::validateKVSize(const memory &key, const memory &value)
{
    uint32_t maxSize = m_be.maxBlockSize();
    if (key.size() + value.size() > maxSize)
        throw std::runtime_error("Key/value too large to insert, max size: " + to_string(maxSize));
}

splitresult_t mutable_tree::insertRec(const node_ptr &node, const memory &key, const memory &value)
{
    if (node->isLeafNode())
    {
        leafnode_ptr leaf = boost::static_pointer_cast<LeafNode>(node);

        keycount_t i = FindLeafKey(leaf, key, m_fns);
        leaf->insert(i, kv_pair(key, value));

        if (NodeSize(leaf, 0, leaf->count()) <= m_be.maxBlockSize())
            return splitresult_t(leaf->count());

        keycount_t j = FindSplit(leaf);
        leafnode_ptr left = boost::make_shared<LeafNode>(leaf->pairs().begin(), leaf->pairs().begin() + j);
        leafnode_ptr right = boost::make_shared<LeafNode>(leaf->pairs().begin() + j, leaf->pairs().end());

        return splitresult_t(left, left->count(),
                             right->minKey(),
                             right, right->count());
    }
    else
    {
        internalnode_ptr internal = boost::static_pointer_cast<InternalNode>(node);

        keycount_t i = FindInternalKey(internal, key, m_fns);
        node_branch &leftBranch = internal->branches()[i];

        splitresult_t childSplit = insertRec(child(leftBranch), key, value);

        // Child didn't split
        if (!childSplit.didSplit)
        {
            leftBranch.itemCount = childSplit.leftCount;
            return splitresult_t(internal->itemCount());
        }

        // Child did split, replace single branch with two branches (in effect,
        // treat current as left and add one to the right).
        node_branch rightBranch(childSplit.splitKey, 0, childSplit.rightCount, childSplit.right);
        leftBranch.itemCount = childSplit.leftCount;
        leftBranch.child = childSplit.left;
        internal->insert(i + 1, rightBranch);

        // Maybe split this leaf
        if (NodeSize(internal, 0, internal->count()) <= m_be.maxBlockSize())
            return splitresult_t(internal->itemCount());

        keycount_t j = FindSplit(internal);
        internalnode_ptr left = boost::make_shared<InternalNode>(internal->branches().begin(), internal->branches().begin() + j);
        internalnode_ptr right = boost::make_shared<InternalNode>(internal->branches().begin() + j, internal->branches().end());

        return splitresult_t(left, left->count(),
                             right->minKey(),
                             right, right->count());
    }
}

void mutable_tree::checkNotFrozen()
{
    if (m_frozen)
        throw std::runtime_error("Can't mutate tree anymore; already flushed");
}

void mutable_tree::flush()
{
    m_frozen = true;

    collectNewIDsRec(root());

    m_be.newIdentifiers(m_newIDsRequired).swap(m_newIDs); // Swaptimization

    m_putBlocks.reserve(m_newIDsRequired);
    collectBlocksToPutRec(root(), m_newIDs[0]);

    m_be.put_all(m_putBlocks); // FIXME: Some of these might fail. Deal with it.
}

/**
 * Traverse the tree, and record that we need a new ID for every node that was loaded
 *
 * Every loaded node got loaded because we changed it, so it needs to be
 * written to a new page. Every branch with a child pointer gets its nodeId
 * replaced with an index into the m_nodeIDs array. After provisioning the new
 * IDs, that array will have the actual IDs for every page.
 */
void mutable_tree::collectNewIDsRec(node_ptr &node)
{
    // FIXME: Check if we actually did any changes. Otherwise we have nothing to write.

    m_newIDsRequired = 1; // For the root

    if (node->isLeafNode())
        return;

    internalnode_ptr internal = boost::static_pointer_cast<InternalNode>(node);
    BOOST_FOREACH(node_branch &b, internal->branches())
    {
        if (b.child)
        {
            b.nodeId = m_newIDsRequired++;
            collectNewIDsRec(b.child);
        }
    }
}

void mutable_tree::collectBlocksToPutRec(node_ptr &node, nodeid_t id)
{
    if (!node->isLeafNode())
    {
        internalnode_ptr internal = boost::static_pointer_cast<InternalNode>(node);
        BOOST_FOREACH(node_branch &b, internal->branches())
        {
            if (b.child)
            {
                // In this case, nodeID is an index into the array of IDs
                b.nodeId = m_newIDs[b.nodeId];
                collectBlocksToPutRec(b.child, b.nodeId);
            }
        }
    }

    // Serialize this node
    m_putBlocks.push_back(be::putblock_t(id, SerializeNode(node)));
}


}
