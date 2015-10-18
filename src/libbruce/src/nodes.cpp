#include "nodes.h"

#include <algorithm>
#include <boost/foreach.hpp>

namespace libbruce {

memslice g_emptyMemory;

//----------------------------------------------------------------------

Node::Node(node_type_t nodeType)
    : m_nodeType(nodeType)
{
}

Node::~Node()
{
}

//----------------------------------------------------------------------

LeafNode::LeafNode(const tree_functions &fns)
    : Node(TYPE_LEAF), pairs(KeyOrder(fns)), m_elementsSize(0)
{
}

LeafNode::LeafNode(pairlist_t::const_iterator begin, pairlist_t::const_iterator end, const tree_functions &fns)
    : Node(TYPE_LEAF), pairs(begin, end, KeyOrder(fns)), m_elementsSize(0)
{
    calcSize();
}

LeafNode::LeafNode(std::vector<kv_pair>::const_iterator begin, std::vector<kv_pair>::const_iterator end, const tree_functions &fns)
    // Do a guaranteed ordered map construction (faster than individual inserts)
    : Node(TYPE_LEAF), pairs(boost::container::ordered_range_t(), begin, end, KeyOrder(fns)), m_elementsSize(0)
{
    calcSize();
}

void LeafNode::calcSize()
{
    for (pairlist_t::const_iterator it = pairs.begin(); it != pairs.end(); ++it)
    {
        m_elementsSize += it->first.size() + it->second.size();
    }
}

const memslice &LeafNode::minKey() const
{
    if (pairs.size()) return pairs.begin()->first;
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

pairlist_t::const_iterator LeafNode::get_at(int n) const
{
    pairlist_t::const_iterator it = pairs.begin();
    for (int i = 0; i < n && it != pairs.end(); ++it, ++i);
    return it;
}

pairlist_t::iterator LeafNode::get_at(int n)
{
    pairlist_t::iterator it = pairs.begin();
    for (int i = 0; i < n && it != pairs.end(); ++it, ++i);
    return it;
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
        assert(it->first == begin->first); // Make sure all have the same key
        values.push_back(it->second);
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

const memslice &OverflowNode::minKey() const
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

const memslice &InternalNode::minKey() const
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

    bool operator()(const memslice &key, const kv_pair &pair) const
    {
        if (key.empty()) return true;
        if (pair.first.empty()) return false;

        return fns.keyCompare(key, pair.first) < 0;
    }

    bool operator()(const kv_pair &pair, const memslice &key) const
    {
        if (pair.first.empty()) return true;
        if (key.empty()) return false;

        return fns.keyCompare(pair.first, key) < 0;
    }

    bool operator()(const node_branch &branch, const memslice &key) const
    {
        if (branch.minKey.empty()) return true;
        if (key.empty()) return false;

        return fns.keyCompare(branch.minKey, key) < 0;
    }
};


keycount_t FindInternalKey(const internalnode_ptr &node, const memslice &key, const tree_functions &fns)
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

keycount_t FindShallowestInternalKey(const internalnode_ptr &node, const memslice &key, const tree_functions &fns)
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
            os << "  " << p.first << " -> " << p.second << std::endl;
        if (!l.overflow.empty())
            os << "  Overflow " << l.overflow.count << " @ " << l.overflow.nodeID << std::endl;
    }
    else if (x.nodeType() == libbruce::TYPE_OVERFLOW)
    {
        const libbruce::OverflowNode &o = (const libbruce::OverflowNode&)x;
        os << "OVERFLOW(" << o.valueCount() << ")" << std::endl;
        BOOST_FOREACH(const libbruce::memslice &m, o.values)
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
