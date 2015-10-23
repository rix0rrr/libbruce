#pragma once
#ifndef BRUCE_QUERY_TREE_H
#define BRUCE_QUERY_TREE_H

#include <map>
#include <vector>
#include <boost/enable_shared_from_this.hpp>

#include <libbruce/types.h>
#include <libbruce/be/be.h>
#include <libbruce/query_tree.h>

#include "tree_impl.h"
#include "nodes.h"
#include "query_iterator_impl.h"

namespace libbruce {

struct callback_memcmp
{
    callback_memcmp(const tree_functions &fns) : fns(fns) { }

    bool operator()(const memslice &a, const memslice &b) const
    {
        if (a.empty()) return true;
        if (b.empty()) return false;

        return fns.keyCompare(a, b) < 0;
    }

    tree_functions fns;
};

struct query_tree_impl : public tree_impl, public boost::enable_shared_from_this<query_tree_impl>
{
    query_tree_impl(be::be &be, nodeid_t rootID, mempool &mempool, const tree_functions &fns);

    void queue_insert(const memslice &key, const memslice &value);
    void queue_upsert(const memslice &key, const memslice &value, bool guaranteed);
    void queue_remove(const memslice &key, bool guaranteed);
    void queue_remove(const memslice &key, const memslice &value, bool guaranteed);

    bool get(const memslice &key, memslice *value);
    query_iterator_impl_ptr find(const memslice &key);
    query_iterator_impl_ptr seek(itemcount_t n);
    query_iterator_impl_ptr begin();

    itemcount_t rank(treepath_t &rootPath);

    fork travelDown(const fork &top, keycount_t i);
    void applyPendingEdits(const internalnode_ptr &internal, fork &fork, node_branch &branch, Depth depth);
private:
    void findRec(treepath_t &rootPath, const memslice *key, query_iterator_impl_ptr *iter_ptr);
    void seekRec(treepath_t &rootPath, itemcount_t n, query_iterator_impl_ptr *iter_ptr);
    bool isGuaranteed(const editlist_t::iterator &cur, const editlist_t::iterator &end);
    itemcount_t rankRec(const treepath_t &rootPath, unsigned i);
    int pendingRankDelta(const internalnode_ptr &node, fork &top, node_branch &branch);
    void findPendingEdits(const internalnode_ptr &internal, fork &fork,
                          editlist_t::iterator *editBegin, editlist_t::iterator *editEnd);
};

}

#endif
