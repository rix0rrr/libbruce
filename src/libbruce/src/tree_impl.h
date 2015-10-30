#pragma once
#ifndef BRUCE_TREE_IMPL_H
#define BRUCE_TREE_IMPL_H

#include <boost/enable_shared_from_this.hpp>

#include <libbruce/be/be.h>
#include <libbruce/mempool.h>
#include <libbruce/types.h>
#include <libbruce/mutation.h>
#include "internal_node.h"
#include "tree_iterator_impl.h"
#include "leaf_node.h"
#include "nodes.h"
#include "priv_types.h"

namespace libbruce {

struct tree_impl : public boost::enable_shared_from_this<tree_impl>
{
    tree_impl(be::be &be, maybe_nodeid rootID, mempool &mempool, const tree_functions &fns);

    void insert(const memslice &key, const memslice &value);
    void upsert(const memslice &key, const memslice &value, bool guaranteed);
    void remove(const memslice &key, bool guaranteed);
    void remove(const memslice &key, const memslice &value, bool guaranteed);

    /**
     * Flush changes to the block engine (this only writes new blocks).
     *
     * Returns a mutation containing the IDs of the blocks that can be garbage
     * collected. After calling this, edit_tree_impl is frozen.
     */
    mutation write();

    bool get(const memslice &key, memslice *value);
    tree_iterator_impl_ptr find(const memslice &key);
    tree_iterator_impl_ptr seek(itemcount_t n);
    tree_iterator_impl_ptr begin();

    itemcount_t rank(treepath_t &rootPath);

    fork travelDown(const fork &top, keycount_t i);
    void applyPendingEdits(const internalnode_ptr &internal, fork &fork, node_branch &branch, Depth depth);

    const node_ptr &child(node_branch &branch);
    const node_ptr &overflowNode(overflow_t &leaf);

private:
    be::be &m_be;
    maybe_nodeid m_rootID;
    mempool &m_mempool;
    tree_functions m_fns;

    std::vector<nodeid_t> m_loadedIDs;
    node_ptr m_root;

    node_ptr &root();

    node_ptr load(nodeid_t id);
    node_ptr deserialize(const mempage &page);

    void apply(const pending_edit &edit, Depth depth);
    void apply(const node_ptr &node, const pending_edit &edit, Depth depth);

    void applyLeaf(const leafnode_ptr &leaf, const pending_edit &edit, int *delta);

    void leafInsert(const leafnode_ptr &node, const memslice &key, const memslice &value, bool upsert, uint32_t *delta);
    void leafRemove(const leafnode_ptr &leaf, const memslice &key, const memslice *value, uint32_t *delta);
    void overflowInsert(overflow_t &overflow_rec, const memslice &value, uint32_t *delta);
    void overflowRemove(overflow_t &overflow_rec, const memslice *value, uint32_t *delta);
    memslice overflowPull(overflow_t &overflow_rec);
    void applyEditsToBranch(const internalnode_ptr &internal, const keycount_t &i);

    be::putblocklist_t m_putBlocks;

    void validateKVSize(const memslice &key, const memslice &value);

    splitresult_t flushAndSplitRec(node_ptr &node);
    nodeid_t collectBlocksRec(node_ptr &node);

    mutation collectMutation();

    void pushDownOverflowNodeSize(const overflownode_ptr &overflow);
    splitresult_t maybeSplitLeaf(const leafnode_ptr &leaf);
    void maybeApplyEdits(const internalnode_ptr &internal);
    splitresult_t maybeSplitInternal(const internalnode_ptr &internal);

    branchlist_t::iterator updateBranch(const internalnode_ptr &internal, branchlist_t::iterator i, const splitresult_t &split);
    be::blockidlist_t findBlocksToFetch(const internalnode_ptr &internal);
    void loadBlocksToEdit(const internalnode_ptr &internal);

    void findRec(treepath_t &rootPath, const memslice *key, tree_iterator_impl_ptr *iter_ptr);
    void seekRec(treepath_t &rootPath, itemcount_t n, tree_iterator_impl_ptr *iter_ptr);
    bool isGuaranteed(const editlist_t::iterator &cur, const editlist_t::iterator &end);
    itemcount_t rankRec(const treepath_t &rootPath, unsigned i);
    int pendingRankDelta(const internalnode_ptr &node, fork &top, node_branch &branch);
    void findPendingEdits(const internalnode_ptr &internal, fork &fork,
                          editlist_t::iterator *editBegin, editlist_t::iterator *editEnd);
};

}

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
