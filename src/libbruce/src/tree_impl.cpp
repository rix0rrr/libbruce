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

index_range tree_impl::findInternalRange(const internalnode_ptr &internal, const memory &key)
{
    keycount_t start = FindInternalKey(internal, key, m_fns); // This is on or after the given key
    keycount_t end = start;

    while (end < internal->branches().size() && safeCompare(internal->branch(end).minKey, key) <= 0)
        end++;

    return index_range(start, end);
}

int tree_impl::safeCompare(const memory &a, const memory &b)
{
    if (a.empty()) return -1;
    if (b.empty()) return 1;
    return m_fns.keyCompare(a, b);
}

}

std::ostream &operator <<(std::ostream &os, const bruce::index_range &r)
{
    os << "[" << r.start << ".." << r.end << ")";
    return os;
}

