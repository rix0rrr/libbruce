#include "tree_impl.h"

#include "serializing.h"

namespace bruce {

tree_impl::tree_impl(be::be &be, maybe_nodeid rootID, const tree_functions &fns)
    : m_be(be), m_rootID(rootID), m_fns(fns)
{
}

node_ptr &tree_impl::root()
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
    memory mem = m_be.get(id);
    return ParseNode(mem, m_fns);
}

index_range tree_impl::findLeafRange(const leafnode_ptr &leaf, const memory &key)
{
    keycount_t end = FindLeafKey(leaf, key, m_fns); // This is AFTER the given key
    keycount_t start = end;

    while (start > 0 && m_fns.keyCompare(leaf->pair(start-1).key, key) == 0)
        start--;

    return index_range(start, end);
}

int tree_impl::safeCompare(const memory &a, const memory &b)
{
    if (a.empty()) return -1;
    if (b.empty()) return 1;
    return m_fns.keyCompare(a, b);
}

void tree_impl::removeFromLeaf(const leafnode_ptr &leaf, const memory &key, const memory *value)
{
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
        }
    }
    else if (!leaf->overflow.empty() && key == leaf->pairs.back().key)
    {
        // Did not erase from this leaf but key matches overflow key, recurse
        removeFromOverflow(overflowNode(leaf->overflow), key, value);
        leaf->setOverflow(leaf->overflow.node);
    }
}

void tree_impl::removeFromOverflow(const node_ptr &node, const memory &key, const memory *value)
{
    overflownode_ptr overflow = boost::static_pointer_cast<OverflowNode>(node);

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
        removeFromOverflow(overflowNode(overflow->next), key, value);
        overflow->setNext(overflow->next.node);
    }

    // If this block is now empty, pull a value from the next one
    if (!overflow->itemCount() && !overflow->next.empty())
    {
        memory value = pullFromOverflow(overflowNode(overflow->next));
        overflow->append(value);
    }
}


memory tree_impl::pullFromOverflow(const node_ptr &node)
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


}

std::ostream &operator <<(std::ostream &os, const bruce::index_range &r)
{
    os << "[" << r.start << ".." << r.end << ")";
    return os;
}

