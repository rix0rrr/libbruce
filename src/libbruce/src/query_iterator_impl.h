#ifndef QUERY_ITERATOR_IMPL_H
#define QUERY_ITERATOR_IMPL_H

#include <libbruce/query_tree.h>

#include "nodes.h"

namespace bruce {

struct knuckle
{
    knuckle() : index(0) {}
    knuckle(node_ptr node, keycount_t index, const memory &minKey, const memory &maxKey)
        : node(node), index(index), minKey(minKey), maxKey(maxKey) { }

    node_ptr node;
    keycount_t index;
    memory minKey;
    memory maxKey;
};

struct query_iterator_impl
{
    query_iterator_impl(query_tree_impl_ptr tree, const std::vector<knuckle> &rootPath, itemcount_t rank);

    const memory &key() const;
    const memory &value() const;
    itemcount_t rank() const;
    bool valid() const;

    void skip(itemcount_t n);
    void next();
private:
    query_tree_impl_ptr m_tree;
    std::vector<knuckle> m_rootPath;
    knuckle m_overflow;
    itemcount_t m_rank;

    leafnode_ptr leaf() const { return boost::static_pointer_cast<LeafNode>(m_rootPath.back().node); }
    keycount_t leafIndex() const { return m_rootPath.back().index; }

    void setCurrentOverflow(const node_ptr &overflow);
    bool inOverflow() const { return m_overflow.node; }
    overflownode_ptr overflow() const { return boost::static_pointer_cast<OverflowNode>(m_overflow.node); }
    keycount_t overflowIndex() const { return m_overflow.index; }

    void advanceCurrent();
    bool pastCurrentEnd() const;
    void popCurrentNode();
    void travelToNextLeaf();
};

}

#endif
