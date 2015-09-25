#include "query_iterator_impl.h"
#include "query_tree_impl.h"

#include "helpers.h"

namespace bruce {

bool knuckle::operator==(const knuckle &other) const
{
    return node == other.node && index == other.index;
}

node_type_t knuckle::nodeType() const
{
    return node->nodeType();
}

internalnode_ptr knuckle::asInternal() const
{
    return boost::static_pointer_cast<InternalNode>(node);
}

leafnode_ptr knuckle::asLeaf() const
{
    return boost::static_pointer_cast<LeafNode>(node);
}

overflownode_ptr knuckle::asOverflow() const
{
    return boost::static_pointer_cast<OverflowNode>(node);
}

query_iterator_impl::query_iterator_impl(query_tree_impl_ptr tree, const std::vector<knuckle> &rootPath)
    : m_tree(tree), m_rootPath(rootPath)
{
}

const knuckle &query_iterator_impl::leaf() const
{
    // Get the top leaf
    for (std::vector<knuckle>::const_reverse_iterator it = m_rootPath.rbegin(); it != m_rootPath.rend(); ++it)
        if (it->nodeType() == TYPE_LEAF)
            return *it;
    return m_rootPath.back();
}

const memory &query_iterator_impl::key() const
{
    switch (current().nodeType())
    {
        case TYPE_LEAF: return current().asLeaf()->pairs[current().index].key;
        case TYPE_OVERFLOW: return current().asLeaf()->pairs.back().key;  // Because we've already exceeded the index at that level
        default: throw std::runtime_error("Illegal case");
    }
}

const memory &query_iterator_impl::value() const
{
    switch (current().nodeType())
    {
        case TYPE_OVERFLOW: return current().asOverflow()->values[current().index];
        case TYPE_LEAF: return current().asLeaf()->pairs[current().index].value;
        default: throw std::runtime_error("Illegal case");
    }
}

itemcount_t query_iterator_impl::rank() const
{
    return m_tree->rank(m_rootPath);
}

bool query_iterator_impl::valid() const
{
    if (!m_rootPath.size()) return false;

    switch (current().nodeType())
    {
        case TYPE_LEAF: return current().index < current().asLeaf()->pairCount();
        case TYPE_INTERNAL: return current().index < current().asInternal()->branchCount();
        case TYPE_OVERFLOW: return current().index < current().asOverflow()->valueCount();
        default: throw std::runtime_error("Illegal case");
    }
}

void query_iterator_impl::skip(itemcount_t n)
{
    assert(false);
}

bool query_iterator_impl::pastCurrentEnd() const
{
    switch (current().nodeType())
    {
        case TYPE_OVERFLOW: return current().asOverflow()->values.size() <= current().index;
        case TYPE_LEAF: return current().asLeaf()->pairs.size() <= current().index;
        default: throw std::runtime_error("Illegal case");
    }
}

void query_iterator_impl::next()
{
    current().index++;
    if (pastCurrentEnd()) advanceCurrent();
}

void query_iterator_impl::advanceCurrent()
{
    // Move on to overflow chain
    if (current().nodeType() == TYPE_OVERFLOW && !current().asOverflow()->next.empty())
    {
        pushOverflow(m_tree->overflowNode(current().asOverflow()->next));
        return;
    }
    if (current().nodeType() == TYPE_LEAF && !current().asLeaf()->overflow.empty())
    {
        pushOverflow(m_tree->overflowNode(current().asLeaf()->overflow));
        return;
    }

    // Nope, pop the root path
    popOverflows();
    popCurrentNode();
    travelToNextLeaf();
}

void query_iterator_impl::popCurrentNode()
{
    m_rootPath.pop_back();
    if (!m_rootPath.size()) return;

    m_rootPath.back().index++;
}

void query_iterator_impl::travelToNextLeaf()
{
    while (m_rootPath.size() && current().nodeType() == TYPE_INTERNAL)
    {
        internalnode_ptr internal = current().asInternal();

        if (current().index < internal->branchCount())
        {
            node_ptr next = m_tree->child(internal->branch(current().index));

            const memory &minK = internal->branch(current().index).minKey.size() ? internal->branch(current().index).minKey : current().minKey;
            const memory &maxK = current().index < internal->branchCount() - 1 ? internal->branch(current().index+1).minKey : current().maxKey;

            m_rootPath.push_back(knuckle(next, 0, minK, maxK));
        }
        else
            popCurrentNode();
    }

    if (m_rootPath.size() && current().nodeType() == TYPE_LEAF)
    {
        m_tree->applyPendingChanges(current().minKey, current().maxKey, NULL);
    }
}

void query_iterator_impl::pushOverflow(const node_ptr &overflow)
{
    m_rootPath.push_back(knuckle(overflow, 0, memory(), memory()));
}

void query_iterator_impl::popOverflows()
{
    while (current().nodeType() == TYPE_OVERFLOW)
        m_rootPath.pop_back();
}

bool query_iterator_impl::operator==(const query_iterator_impl &other) const
{
    if (m_tree != other.m_tree) return false;
    if (m_rootPath.size() != other.m_rootPath.size()) return false;

    for (unsigned i = 0; i < m_rootPath.size(); i++)
        if (m_rootPath[i] != other.m_rootPath[i])
            return false;

    if (m_overflow != other.m_overflow)
        return false;

    return true;
}

}
