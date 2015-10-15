#pragma once
#ifndef BRUCE_TREE_IMPL_H
#define BRUCE_TREE_IMPL_H

#include <libbruce/be/be.h>
#include <libbruce/types.h>
#include "nodes.h"

namespace libbruce {

struct index_range
{
    index_range(keycount_t start, keycount_t end)
        : start(start), end(end) { }

    keycount_t start;
    keycount_t end;
};

struct tree_impl
{
    tree_impl(be::be &be, maybe_nodeid rootID, const tree_functions &fns);

    node_ptr &root();

    const node_ptr &child(node_branch &branch);
    const node_ptr &overflowNode(overflow_t &leaf);
protected:
    be::be &m_be;
    maybe_nodeid m_rootID;
    tree_functions m_fns;

    std::vector<nodeid_t> m_loadedIDs;
    node_ptr m_root;

    node_ptr load(nodeid_t id);

    int safeCompare(const memory &a, const memory &b);
    void findLeafRange(const leafnode_ptr &leaf, const memory &key, pairlist_t::iterator *begin, pairlist_t::iterator *end);

    memory pullFromOverflow(const node_ptr &node);
    bool removeFromLeaf(const leafnode_ptr &leaf, const memory &key, const memory *value, pairlist_t::iterator *leafIter);
    bool removeFromOverflow(const node_ptr &node, const memory &key, const memory *value);
};

}

std::ostream &operator <<(std::ostream &os, const libbruce::index_range &r);

#define NODE_CASE_LEAF \
    if (node->nodeType() == TYPE_LEAF) \
    { \
        leafnode_ptr leaf = boost::static_pointer_cast<LeafNode>(node);

#define NODE_CASE_OVERFLOW \
    } \
    else if (node->nodeType() == TYPE_OVERFLOW) \
    { \
        overflownode_ptr overflow = boost::static_pointer_cast<OverflowNode>(node);

#define NODE_CASE_INT \
    } \
    else \
    { \
        internalnode_ptr internal = boost::static_pointer_cast<InternalNode>(node);

#define NODE_CASE_END \
    }

#endif
