#include "query_iterator_impl.h"
#include "query_tree_impl.h"

#include "helpers.h"

namespace bruce {

bool knuckle::operator==(const knuckle &other) const
{
    return node == other.node && index == other.index;
}

bool knuckle::isLeaf() const
{
    return node->nodeType() == TYPE_LEAF;
}

internalnode_ptr knuckle::asInternal() const
{
    return boost::static_pointer_cast<InternalNode>(node);
}

leafnode_ptr knuckle::asLeaf() const
{
    return boost::static_pointer_cast<LeafNode>(node);
}

query_iterator_impl::query_iterator_impl(query_tree_impl_ptr tree, const std::vector<knuckle> &rootPath, itemcount_t rank)
    : m_tree(tree), m_rootPath(rootPath), m_rank(rank)
{
}

const memory &query_iterator_impl::key() const
{
    if (inOverflow())
        return leaf()->pairs.back().key;
    return leaf()->pairs[leafIndex()].key;
}

const memory &query_iterator_impl::value() const
{
    if (inOverflow())
        return overflow()->values[overflowIndex()];
    return leaf()->pairs[leafIndex()].value;
}

itemcount_t query_iterator_impl::rank() const
{
    assert(false);
}

bool query_iterator_impl::valid() const
{
    if (!m_rootPath.size()) return false;

    const knuckle &k = m_rootPath.back();

    return IMPLIES(k.isLeaf(), k.index < k.asLeaf()->pairCount())
        && IMPLIES(!k.isLeaf(), k.index < k.asInternal()->branchCount());
}

void query_iterator_impl::skip(itemcount_t n)
{
    assert(false);
}

bool query_iterator_impl::pastCurrentEnd() const
{
    if (inOverflow())
        return overflow()->values.size() <= overflowIndex();
    return leaf()->pairs.size() <= leafIndex();
}

void query_iterator_impl::next()
{
    if (inOverflow())
        m_overflow.index++;
    else
        m_rootPath.back().index++;
    if (pastCurrentEnd()) advanceCurrent();
}

void query_iterator_impl::advanceCurrent()
{
    // Move on to overflow chain
    if (inOverflow() && !overflow()->next.empty())
    {
        setCurrentOverflow(m_tree->overflowNode(overflow()->next));
        return;
    }
    if (!inOverflow() && !leaf()->overflow.empty())
    {
        setCurrentOverflow(m_tree->overflowNode(leaf()->overflow));
        return;
    }

    // Nope, pop the root path
    setCurrentOverflow(node_ptr());
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
    while (m_rootPath.size() && m_rootPath.back().node->nodeType() == TYPE_INTERNAL)
    {
        const knuckle &k = m_rootPath.back();
        internalnode_ptr internal = boost::static_pointer_cast<InternalNode>(k.node);

        if (k.index < internal->branchCount())
        {
            node_ptr next = m_tree->child(internal->branch(k.index));

            const memory &minK = internal->branch(k.index).minKey.size() ? internal->branch(k.index).minKey : k.minKey;
            const memory &maxK = k.index < internal->branchCount() - 1 ? internal->branch(k.index+1).minKey : k.maxKey;

            m_rootPath.push_back(knuckle(next, 0, minK, maxK));
        }
        else
            popCurrentNode();
    }

    if (m_rootPath.size() && m_rootPath.back().node->nodeType() == TYPE_LEAF)
    {
        const knuckle &k = m_rootPath.back();
        leafnode_ptr leaf = boost::static_pointer_cast<LeafNode>(k.node);
        m_tree->applyPendingChanges(k.minKey, k.maxKey);
    }
}

void query_iterator_impl::setCurrentOverflow(const node_ptr &overflow)
{
    m_overflow.node = overflow;
    m_overflow.index = 0;
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
