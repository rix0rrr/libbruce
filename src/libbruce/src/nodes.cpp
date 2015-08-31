#include "nodes.h"

#include <algorithm>
#include <boost/foreach.hpp>

namespace bruce {

Node::~Node()
{
}

LeafNode::LeafNode(size_t sizeHint)
{
    if (sizeHint) m_pairs.reserve(sizeHint);
}

LeafNode::LeafNode(pairlist_t::iterator begin, pairlist_t::iterator end)
    : m_pairs(begin, end)
{
}

const memory &LeafNode::minKey() const
{
    return m_pairs[0].key;
}

keycount_t LeafNode::itemCount() const
{
    return m_pairs.size();
}

InternalNode::InternalNode(size_t sizeHint)
{
    if (sizeHint) m_branches.reserve(sizeHint);
}

InternalNode::InternalNode(branchlist_t::iterator begin, branchlist_t::iterator end)
    : m_branches(begin, end)
{
}

keycount_t InternalNode::itemCount() const
{
    keycount_t ret = 0;
    for (branchlist_t::const_iterator it = m_branches.begin(); it != m_branches.end(); ++it)
    {
        ret += it->itemCount;
    }
    return ret;
}

const memory &InternalNode::minKey() const
{
    return m_branches[0].minKey;
}

struct KeyCompare
{
    KeyCompare(const tree_functions &fns) : fns(fns) { }
    const tree_functions &fns;

    bool operator()(const memory &key, const kv_pair &pair)
    {
        return fns.keyCompare(key, pair.key) < 0;
    }

    bool operator()(const node_branch &branch, const memory &key)
    {
        if (branch.minKey.empty()) return true;

        return fns.keyCompare(branch.minKey, key) < 0;
    }
};

keycount_t FindLeafKey(const leafnode_ptr &leaf, const memory &key, const tree_functions &fns)
{
    return std::upper_bound(leaf->pairs().begin(), leaf->pairs().end(), key, KeyCompare(fns)) - leaf->pairs().begin();
}

keycount_t FindInternalKey(const internalnode_ptr &node, const memory &key, const tree_functions &fns)
{
    // lower_bound: Points to first element which is >= key
    // So we need lower_bound - 1.
    keycount_t i = std::lower_bound(node->branches().begin(), node->branches().end(), key, KeyCompare(fns)) - node->branches().begin();
    return i ? i - 1 : 0;
}

keycount_t FindShallowestInternalKey(const internalnode_ptr &node, const memory &key, const tree_functions &fns)
{
    keycount_t i = FindInternalKey(node, key, fns);
    keycount_t ret = i;
    itemcount_t min_items = node->branch(i).itemCount;
    i++;

    while (i < node->branches().size() && fns.keyCompare(node->branch(i).minKey, key) <= 0)
    {
        if (node->branch(i).itemCount < min_items)
        {
            min_items = node->branch(i).itemCount;
            ret = i;
        }

        i++;
    }

    return ret;
}

}

std::ostream &operator <<(std::ostream &os, const bruce::Node &x)
{
    if (x.isLeafNode())
    {
        const bruce::LeafNode &l = (const bruce::LeafNode&)x;
        os << "LEAF(" << l.count() << ")" << std::endl;
        BOOST_FOREACH(const bruce::kv_pair &p, l.pairs())
            os << "  " << p.key << " -> " << p.value << std::endl;
    }
    else
    {
        const bruce::InternalNode &l = (const bruce::InternalNode&)x;
        os << "INTERNAL(" << l.count() << ")" << std::endl;
        BOOST_FOREACH(const bruce::node_branch &b, l.branches())
            os << "  " << b.minKey << " -> " << b.nodeID << " (" << b.itemCount << ")" << std::endl;
    }
    return os;
}
