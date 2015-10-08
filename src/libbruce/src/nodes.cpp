#include "nodes.h"

#include <algorithm>
#include <boost/foreach.hpp>

namespace libbruce {

memory g_emptyMemory;

//----------------------------------------------------------------------

Node::Node(node_type_t nodeType)
    : m_nodeType(nodeType)
{
}

Node::~Node()
{
}

//----------------------------------------------------------------------

LeafNode::LeafNode(size_t sizeHint)
    : Node(TYPE_LEAF)
{
    if (sizeHint) pairs.reserve(sizeHint);
}

LeafNode::LeafNode(pairlist_t::const_iterator begin, pairlist_t::const_iterator end)
    : Node(TYPE_LEAF), pairs(begin, end)
{
}

const memory &LeafNode::minKey() const
{
    if (pairs.size()) return pairs[0].key;
    return g_emptyMemory;
}

itemcount_t LeafNode::itemCount() const
{
    return overflow.count + pairCount();
}

void LeafNode::setOverflow(const node_ptr &node)
{
    overflow.node = node;
    overflow.count = node->itemCount();
    if (!overflow.count)
    {
        overflow.node = node_ptr();
        overflow.count = 0;
    }
}

//----------------------------------------------------------------------

OverflowNode::OverflowNode(size_t sizeHint)
    : Node(TYPE_OVERFLOW)
{
    if (sizeHint) values.reserve(sizeHint);
}

OverflowNode::OverflowNode(pairlist_t::const_iterator begin, pairlist_t::const_iterator end)
    : Node(TYPE_OVERFLOW)
{
    for (pairlist_t::const_iterator it = begin; it != end; ++it)
    {
        assert(it->key == begin->key); // Make sure all have the same key
        values.push_back(it->value);
    }
}

OverflowNode::OverflowNode(valuelist_t::const_iterator begin, valuelist_t::const_iterator end)
    : Node(TYPE_OVERFLOW), values(begin, end)
{
}

itemcount_t OverflowNode::itemCount() const
{
    return next.count + valueCount();
}

const memory &OverflowNode::minKey() const
{
    if (values.size()) return values[0];
    return g_emptyMemory;
}

void OverflowNode::setNext(const node_ptr &node)
{
    next.node = node;
    next.count = node->itemCount();
    if (!next.count)
    {
        next.node = node_ptr();
        next.count = 0;
    }
}

//----------------------------------------------------------------------

InternalNode::InternalNode(size_t sizeHint)
    : Node(TYPE_INTERNAL)
{
    if (sizeHint) branches.reserve(sizeHint);
}

InternalNode::InternalNode(branchlist_t::const_iterator begin, branchlist_t::const_iterator end)
    : Node(TYPE_INTERNAL), branches(begin, end)
{
}

itemcount_t InternalNode::itemCount() const
{
    keycount_t ret = 0;
    for (branchlist_t::const_iterator it = branches.begin(); it != branches.end(); ++it)
    {
        ret += it->itemCount;
    }
    return ret;
}

const memory &InternalNode::minKey() const
{
    if (branches.size()) return branches[0].minKey;
    return g_emptyMemory;
}

void InternalNode::setBranch(size_t i, const node_ptr &node)
{
    branches[i].child = node;
    branches[i].itemCount = node->itemCount();
}

//----------------------------------------------------------------------

struct KeyCompare
{
    KeyCompare(const tree_functions &fns) : fns(fns) { }
    const tree_functions &fns;

    bool operator()(const memory &key, const kv_pair &pair)
    {
        if (key.empty()) return true;
        if (pair.key.empty()) return false;

        return fns.keyCompare(key, pair.key) < 0;
    }

    bool operator()(const kv_pair &pair, const memory &key)
    {
        if (pair.key.empty()) return true;
        if (key.empty()) return false;

        return fns.keyCompare(pair.key, key) < 0;
    }

    bool operator()(const node_branch &branch, const memory &key)
    {
        if (branch.minKey.empty()) return true;
        if (key.empty()) return false;

        return fns.keyCompare(branch.minKey, key) < 0;
    }
};

keycount_t FindLeafInsertKey(const leafnode_ptr &leaf, const memory &key, const tree_functions &fns)
{
    return std::upper_bound(leaf->pairs.begin(), leaf->pairs.end(), key, KeyCompare(fns)) - leaf->pairs.begin();
}

keycount_t FindLeafUpsertKey(const leafnode_ptr &leaf, const memory &key, const tree_functions &fns)
{
    return std::lower_bound(leaf->pairs.begin(), leaf->pairs.end(), key, KeyCompare(fns)) - leaf->pairs.begin();
}

keycount_t FindInternalKey(const internalnode_ptr &node, const memory &key, const tree_functions &fns)
{
    // lower_bound: Points to first element which is >= key
    // We need the last key which is not > key
    // So we need lower_bound - 1.
    keycount_t i = std::lower_bound(node->branches.begin(), node->branches.end(), key, KeyCompare(fns)) - node->branches.begin();
    if (i == node->branches.size()) return i - 1;
    if (node->branch(i).minKey.empty()) return i;

    if (i > 0 && fns.keyCompare(node->branch(i).minKey, key) != 0)
        i--;

    assert(i < node->branches.size());

    return i;
}

keycount_t FindShallowestInternalKey(const internalnode_ptr &node, const memory &key, const tree_functions &fns)
{
    keycount_t i = FindInternalKey(node, key, fns);
    keycount_t ret = i;
    itemcount_t min_items = node->branch(i).itemCount;
    i++;

    while (i < node->branches.size() && fns.keyCompare(node->branch(i).minKey, key) <= 0)
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

std::ostream &operator <<(std::ostream &os, const libbruce::Node &x)
{
    if (x.nodeType() == libbruce::TYPE_LEAF)
    {
        const libbruce::LeafNode &l = (const libbruce::LeafNode&)x;
        os << "LEAF(" << l.pairCount() << ")" << std::endl;
        BOOST_FOREACH(const libbruce::kv_pair &p, l.pairs)
            os << "  " << p.key << " -> " << p.value << std::endl;
        if (!l.overflow.empty())
            os << "  Overflow " << l.overflow.count << " @ " << l.overflow.nodeID << std::endl;
    }
    else if (x.nodeType() == libbruce::TYPE_OVERFLOW)
    {
        const libbruce::OverflowNode &o = (const libbruce::OverflowNode&)x;
        os << "OVERFLOW(" << o.valueCount() << ")" << std::endl;
        BOOST_FOREACH(const libbruce::memory &m, o.values)
            os << "  " << m << std::endl;
        if (!o.next.empty())
            os << "  Next " << o.next.count << " @ " << o.next.nodeID << std::endl;
    }
    else
    {
        const libbruce::InternalNode &l = (const libbruce::InternalNode&)x;
        os << "INTERNAL(" << l.branchCount() << ")" << std::endl;
        BOOST_FOREACH(const libbruce::node_branch &b, l.branches)
            os << "  " << b.minKey << " -> " << b.nodeID << " (" << b.itemCount << (b.child ? "*" : "") << ")" << std::endl;
    }
    return os;
}

}
