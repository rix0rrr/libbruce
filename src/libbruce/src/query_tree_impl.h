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

    void applyPendingChanges(const memslice &minKey, const memslice &maxKey, int *delta);
    itemcount_t rank(treepath_t &rootPath);
private:
    typedef std::map<memslice, editlist_t, callback_memcmp> editmap_t;

    editmap_t m_edits;

    void pushChildFork(treepath_t &rootPath);
    void findRec(treepath_t &rootPath, const memslice *key, query_iterator_impl_ptr *iter_ptr);
    void seekRec(treepath_t &rootPath, itemcount_t n, query_iterator_impl_ptr *iter_ptr);
    void applyPendingChangeRec(const node_ptr &node, const pending_edit &edit, int *delta);
    bool isGuaranteed(const editlist_t::iterator &cur, const editlist_t::iterator &end);
    itemcount_t rankRec(const treepath_t &rootPath, unsigned i);
    int pendingRankDelta(const node_ptr &node, const memslice &minKey, const memslice &maxKey);
};

}

#endif
