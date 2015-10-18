#pragma once
#ifndef QUERY_ITERATOR_IMPL_H
#define QUERY_ITERATOR_IMPL_H

#include <libbruce/query_tree.h>

#include "nodes.h"

namespace libbruce {

struct knuckle
{
    knuckle() : index(0) {}
    knuckle(node_ptr node, const memory &minKey, const memory &maxKey)
        : node(node), index(0), minKey(minKey), maxKey(maxKey)
    {
        if (nodeType() == TYPE_LEAF) leafIter = asLeaf()->pairs.begin();
    }

    keycount_t leafIndex() const
    {
        return leafIter - asLeaf()->pairs.begin();
    }

    void setLeafIndex(keycount_t index)
    {
        leafIter = asLeaf()->pairs.begin() + index;
    }

    node_ptr node;
    pairlist_t::iterator leafIter;
    keycount_t index;
    memory minKey;
    memory maxKey;

    node_type_t nodeType() const;
    internalnode_ptr asInternal() const;
    leafnode_ptr asLeaf() const;
    overflownode_ptr asOverflow() const;

    bool operator==(const knuckle &other) const;
    bool operator!=(const knuckle &other) const { return !(*this == other); }
};

typedef std::vector<knuckle> treepath_t;

struct query_iterator_impl
{
    query_iterator_impl(query_tree_impl_ptr tree, const treepath_t &rootPath);

    const memory &key() const;
    const memory &value() const;
    itemcount_t rank() const;
    bool valid() const;

    void skip(int n);
    void next();

    bool operator==(const query_iterator_impl &other) const;
    bool operator!=(const query_iterator_impl &other) const { return !(*this == other); }
private:
    query_tree_impl_ptr m_tree;
    mutable treepath_t m_rootPath;

    const knuckle &leaf() const;
    knuckle &current() { return m_rootPath.back(); }
    const knuckle &current() const { return m_rootPath.back(); }

    void advanceCurrent();
    bool pastCurrentEnd() const;
    void popCurrentNode();
    void travelToNextLeaf();

    void pushOverflow(const node_ptr &overflow);
    void popOverflows();
    bool validIndex(int i) const;
};

}

#endif
