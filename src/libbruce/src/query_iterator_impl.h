#pragma once
#ifndef QUERY_ITERATOR_IMPL_H
#define QUERY_ITERATOR_IMPL_H

#include <libbruce/tree.h>

#include "nodes.h"
#include "leaf_node.h"
#include "internal_node.h"
#include "overflow_node.h"

namespace libbruce {

struct fork
{
    fork() : index(0) {}
    fork(node_ptr node, const memslice &minKey, const memslice &maxKey)
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
    memslice minKey;
    memslice maxKey;

    node_type_t nodeType() const;
    internalnode_ptr asInternal() const;
    leafnode_ptr asLeaf() const;
    overflownode_ptr asOverflow() const;

    bool operator==(const fork &other) const;
    bool operator!=(const fork &other) const { return !(*this == other); }
};

typedef std::vector<fork> treepath_t;

struct query_iterator_impl
{
    query_iterator_impl(tree_impl_ptr tree, const treepath_t &rootPath);

    const memslice &key() const;
    const memslice &value() const;
    itemcount_t rank() const;
    bool valid() const;

    void skip(int n);
    void next();

    bool operator==(const query_iterator_impl &other) const;
    bool operator!=(const query_iterator_impl &other) const { return !(*this == other); }
private:
    tree_impl_ptr m_tree;
    mutable treepath_t m_rootPath;

    const fork &leaf() const;
    fork &current() { return m_rootPath.back(); }
    const fork &current() const { return m_rootPath.back(); }

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
