/**
 * Classes that encapsulate mutating operations
 *
 *
 * MUTATION STRATEGY
 * -----------------
 *
 * Mutating operations translate into identifier requests and puts to the block
 * store. Modifications can be scheduled, so that we minimize the number of
 * roundtrips to a remote block store (such as DynamoDB): one to reserve a set
 * of identifiers, then one to parallel put a number of pages.
 *
 * Because modifications depend on the newly created identifiers, we store
 * symbolic representations of the operations to be performed, which are then
 * executed when the identifiers become available.
 *
 * The operations in the execution machine are:
 *
 * - Copy leaf node (sub)range, optionally insert K/V pair, put under ID X
 * - Copy internal node (sub)range, optionally insert K/ID X/C tuple, put under
 *   ID Y
 *
 *
 * INSERT
 * ------
 *
 * Insert operation is as follows:
 *
 * - Dive down into the leaf node that the value needs to be inserted.
 * - If the item can be added within the block size bounds:
 *      - Create a new leaf node with the KV pair added in the right place.
 *      - For every leaf back up to the root, create a new internal node
 *        with the original page identifier replaced by the new one and
 *        the count increased by 1 (guaranteed fit without checking).
 * - If the block with the item added exceeds size bounds:
 *      - Create two new leaf nodes, splitting the complete item set (include
 *        the newly added item) in the middle. The middle is defined in the
 *        size domain, s.t. the left half has the slightly larger half in
 *        case a clean split is not possible[1].
 *      - Replace the node identifier in the parent with references to the
 *        newly created leaf pages and the minimal key of the leftmost page.
 *      - If this causes the internal node to become too large, the same
 *        splitting-and-replacing procedure applies, until the node references
 *        fit or we're creating a new root.
 *
 * [1] Splitting to the left optimizes (slightly) for the case where we'll be
 * inserting in sorted order.
 *
 */
#ifndef BRUCE_OPERATIONS_H
#define BRUCE_OPERATIONS_H

#include <libbruce/mutation.h>
#include <libbruce/be/be.h>
#include <libbruce/traits.h>
#include <libbruce/edit_tree.h>

#include "nodes.h"
#include "tree_impl.h"

#include <map>

namespace bruce {

struct splitresult_t {
    splitresult_t(itemcount_t leftCount) : didSplit(false), leftCount(leftCount) { }
    splitresult_t(node_ptr left, itemcount_t leftCount, const memory &splitKey, node_ptr right, itemcount_t rightCount)
        : didSplit(true), left(left), leftCount(leftCount), splitKey(splitKey), right(right), rightCount(rightCount) { }

    bool didSplit;
    node_ptr left;
    itemcount_t leftCount;
    memory splitKey;
    node_ptr right;
    itemcount_t rightCount;
};

// FIXME: Reject duplicate inserts?

/**
 * Operations on a tree
 *
 * The tree is loaded into memory on-demand.
 */
struct edit_tree_impl : private tree_impl
{
    edit_tree_impl(be::be &be, maybe_nodeid rootID, const tree_functions &fns);

    /**
     * Insert an item into the tree.
     *
     * The key and value are NOT copied. If the memory slice is borrowed, the
     * borrowed memory must live until this edit_tree_impl has been flushed.  If
     * the memory slice is shared, the shared_ptr will make sure the memory is
     * not released prematurely.
     */
    void insert(const memory &key, const memory &value);

    // Remove any element with the given key
    bool remove(const memory &key);

    // Remove only the element with the given key and value
    bool remove(const memory &key, const memory &value);

    /**
     * Flush changes to the block engine (this only writes new blocks).
     *
     * Returns a mutation containing the IDs of the blocks that can be garbage
     * collected. After calling this, edit_tree_impl is frozen.
     */
    mutation flush();
private:
    bool m_frozen;
    uint32_t m_newIDsRequired;
    std::vector<nodeid_t> m_oldIDs;
    std::vector<nodeid_t> m_newIDs;
    be::putblocklist_t m_putBlocks;

    splitresult_t insertRec(const node_ptr &node, const memory &key, const memory &value);
    itemcount_t removeRec(const node_ptr &node, const memory &key, const memory *value);
    void validateKVSize(const memory &key, const memory &value);
    void checkNotFrozen();

    void collectNewIDsRec(node_ptr &node);
    void collectBlocksToPutRec(node_ptr &node, nodeid_t id);
    mutation collectMutation();
};

}

#endif
