#include "nodes.h"

#include <algorithm>

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

keycount_t FindInternalKey(const internalnode_ptr &leaf, const memory &key, const tree_functions &fns)
{
    return std::lower_bound(leaf->branches().begin(), leaf->branches().end(), key, KeyCompare(fns)) - leaf->branches().begin();
}

}
