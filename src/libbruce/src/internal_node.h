#pragma once
#ifndef INTERNALNODE_H
#define INTERNALNODE_H

#include "nodes.h"
#include "priv_types.h"

namespace libbruce {

/**
 * Internal node type
 */
struct InternalNode : public Node
{
    InternalNode(size_t sizeHint=0);
    InternalNode(branchlist_t::const_iterator begin, branchlist_t::const_iterator end);

    keycount_t branchCount() const { return branches.size(); }
    virtual const memslice &minKey() const;

    void insert(size_t i, const node_branch &branch) { branches.insert(branches.begin() + i, branch); }
    void erase(size_t i) { branches.erase(branches.begin() + i); }
    void append(const node_branch &branch) { branches.push_back(branch); }

    branchlist_t::const_iterator at(keycount_t i) const { return branches.begin() + i; }

    itemcount_t itemCount() const;

    node_branch &branch(keycount_t i) { return branches[i]; }

    void setBranch(size_t i, const node_ptr &node);
    void print(std::ostream &os) const;

    branchlist_t branches;
    editlist_t editQueue;
};

/**
 * Return the index where the subtree for a particular key will be located
 *
 * POST: branch[ret-1] < branch[ret].key <= key
 */
keycount_t FindInternalKey(const internalnode_ptr &node, const memslice &key, const tree_functions &fns);

/**
 * Find the index where this item can go with the least amount of child nodes.
 *
 * POST: branch[ret].key <= key <= branch[ret+1].key
 */
keycount_t FindShallowestInternalKey(const internalnode_ptr &node, const memslice &key, const tree_functions &fns);

}

#endif
