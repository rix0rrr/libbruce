#include "tree_impl.h"

#include "serializing.h"

namespace libbruce {

tree_impl::tree_impl(be::be &be, maybe_nodeid rootID, mempool &mempool, const tree_functions &fns)
    : m_be(be), m_rootID(rootID), m_mempool(mempool), m_fns(fns)
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
    mempage mem = m_be.get(id);
    m_mempool.retain(mem);
    return ParseNode(mem, m_fns);
}

void tree_impl::findLeafRange(const leafnode_ptr &leaf, const memory &key, pairlist_t::iterator *begin, pairlist_t::iterator *end)
{
    *begin = leaf->pairs.lower_bound(key);
    *end = leaf->pairs.upper_bound(key);
}

int tree_impl::safeCompare(const memory &a, const memory &b)
{
    if (a.empty()) return -1;
    if (b.empty()) return 1;
    return m_fns.keyCompare(a, b);
}

bool tree_impl::removeFromLeaf(const leafnode_ptr &leaf, const memory &key, const memory *value)
{
    pairlist_t::iterator begin, end;
    findLeafRange(leaf, key, &begin, &end);

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

        // If we removed the final position, pull back from the overflow block
        // and potentially split.
        if (eraseLocation == leaf->pairs.end() && !leaf->overflow.empty())
        {
            memory ret = pullFromOverflow(leaf->overflow.node);
            leaf->insert(kv_pair(key, ret));
            leaf->setOverflow(leaf->overflow.node);
        }

        return true;
    }

    if (leaf->pairs.empty()) return false;

    // If we did not erase here, but the key matches the last key, search in the overflow block
    if (!leaf->overflow.empty() && key == leaf->pairs.rbegin()->first)
    {
        // Did not erase from this leaf but key matches overflow key, recurse
        bool erased = removeFromOverflow(overflowNode(leaf->overflow), key, value);
        leaf->setOverflow(leaf->overflow.node);
        return erased;
    }

    return false;
}

bool tree_impl::removeFromOverflow(const node_ptr &node, const memory &key, const memory *value)
{
    overflownode_ptr overflow = boost::static_pointer_cast<OverflowNode>(node);

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
        erased = removeFromOverflow(overflowNode(overflow->next), key, value);
        overflow->setNext(overflow->next.node);
    }

    // If this block is now empty, pull a value from the next one
    if (!overflow->itemCount() && !overflow->next.empty())
    {
        memory value = pullFromOverflow(overflowNode(overflow->next));
        overflow->append(value);
    }

    return erased;
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

std::ostream &operator <<(std::ostream &os, const libbruce::index_range &r)
{
    os << "[" << r.start << ".." << r.end << ")";
    return os;
}

