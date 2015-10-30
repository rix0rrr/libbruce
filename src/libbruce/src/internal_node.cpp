#include "internal_node.h"
#include <boost/foreach.hpp>

namespace libbruce {

node_branch::node_branch(const memslice &minKey, nodeid_t nodeID, itemcount_t itemCount)
    : minKey(minKey), nodeID(nodeID), itemCount(itemCount)
{
}

node_branch::node_branch(const memslice &minKey, const node_ptr &child)
    : minKey(minKey), nodeID(), itemCount(child->itemCount()), child(child)
{
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
    for (editlist_t::const_iterator it = editQueue.begin(); it != editQueue.end(); ++it)
    {
        if (it->guaranteed)
            ret += it->delta();
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

void InternalNode::print(std::ostream &os) const
{
    os << "INTERNAL(" << branchCount() << ")" << std::endl;
    BOOST_FOREACH(const libbruce::node_branch &b, branches)
        os << "  " << b.minKey << " -> " << b.nodeID << " (" << b.itemCount << (b.child ? "*" : "") << ")" << std::endl;
    BOOST_FOREACH(const libbruce::pending_edit &e, editQueue)
        os << "  " << e << std::endl;
}

//----------------------------------------------------------------------

struct BranchCompare
{
    BranchCompare(const tree_functions &fns) : fns(fns) { }
    const tree_functions &fns;

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
    keycount_t i = std::lower_bound(node->branches.begin(), node->branches.end(), key, BranchCompare(fns)) - node->branches.begin();
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

}
