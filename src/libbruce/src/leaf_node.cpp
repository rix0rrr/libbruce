#include "leaf_node.h"
#include <boost/foreach.hpp>

namespace libbruce {

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

void LeafNode::print(std::ostream &os) const
{
    os << "LEAF(" << pairCount() << ")" << std::endl;
    BOOST_FOREACH(const libbruce::kv_pair &p, pairs)
        os << "  " << p.first << " -> " << p.second << std::endl;
    if (!overflow.empty())
        os << "  Overflow " << overflow.count << " @ " << overflow.nodeID << std::endl;
}

}
