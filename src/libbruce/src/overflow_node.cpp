#include "overflow_node.h"
#include <boost/foreach.hpp>

namespace libbruce {

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

void OverflowNode::print(std::ostream &os) const
{
    os << "OVERFLOW(" << valueCount() << ")" << std::endl;
    BOOST_FOREACH(const libbruce::memslice &m, values)
        os << "  " << m << std::endl;
    if (!next.empty())
        os << "  Next " << next.count << " @ " << next.nodeID << std::endl;
}

}
