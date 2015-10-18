#include "query_iterator_impl.h"
#include "query_tree_impl.h"

#include "helpers.h"

namespace libbruce {

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
        case TYPE_LEAF: return current().leafIter->first;
        case TYPE_OVERFLOW: return current().asLeaf()->pairs.rbegin()->first;  // Because we've already exceeded the index at that level
        default: throw std::runtime_error("Illegal case");
    }
}

const memory &query_iterator_impl::value() const
{
    switch (current().nodeType())
    {
        case TYPE_OVERFLOW: return current().asOverflow()->values[current().index];
        case TYPE_LEAF: return current().leafIter->second;
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

    if (current().nodeType() == TYPE_LEAF)
        return current().leafIter != current().asLeaf()->pairs.end();

    return validIndex(current().index);
}

bool query_iterator_impl::validIndex(int i) const
{
    switch (current().nodeType())
    {
        case TYPE_LEAF: assert(false); return false;
        case TYPE_INTERNAL: return i < current().asInternal()->branchCount();
        case TYPE_OVERFLOW: return i < current().asOverflow()->valueCount();
        default: throw std::runtime_error("Illegal case");
    }
}

void query_iterator_impl::skip(int n)
{
    // Try to satisfy move by just moving index pointer
    switch (current().nodeType())
    {
        case TYPE_LEAF:
            {
                int i = 0;
                while (i < n && current().leafIter != current().asLeaf()->pairs.end())
                {
                    ++i;
                    ++current().leafIter;
                }
                if (i == n) return; // Success
                break;
            }
        default:
            {
                int potential = current().index + n;
                if (potential <= 0 && validIndex(potential))
                {
                    current().index = potential;
                    return;
                }
                break;
            }
    }

    // Otherwise do a complete re-seek
    *this = *m_tree->seek(rank() + n);
}

bool query_iterator_impl::pastCurrentEnd() const
{
    switch (current().nodeType())
    {
        case TYPE_OVERFLOW: return current().asOverflow()->values.size() <= current().index;
        case TYPE_LEAF: return current().leafIter == current().asLeaf()->pairs.end();
        default: throw std::runtime_error("Illegal case");
    }
}

void query_iterator_impl::next()
{
    if (current().nodeType() == TYPE_LEAF)
    {
        assert(current().leafIter != current().asLeaf()->pairs.end());
        ++current().leafIter;
    }
    else
        ++current().index;
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

            m_rootPath.push_back(knuckle(next, minK, maxK));
        }
        else
            popCurrentNode();
    }

    if (m_rootPath.size() && current().nodeType() == TYPE_LEAF)
    {
        // Be sure to save and restore the iterator
        int index = current().leafIndex();
        m_tree->applyPendingChanges(current().minKey, current().maxKey, NULL);
        current().setLeafIndex(index);
    }
}

void query_iterator_impl::pushOverflow(const node_ptr &overflow)
{
    m_rootPath.push_back(knuckle(overflow, memory(), memory()));
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

    return true;
}

}
