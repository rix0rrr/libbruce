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
#include <libbruce/tree.h>
#include "serializing.h"

namespace bruce {

struct insert_operation
{
    insert_operation(be::be &be, maybe_blockid id, const memory &key, const memory &value, types::comparison_fn *compareKeys);

    mutation mut;

    void do_it();
private:
    maybe_blockid m_id;
    be::be &m_be;
    const memory &m_key;
    const memory &m_value;
    types::comparison_fn *m_compareKeys;

    nodeident_t createNewLeafNode(const nodeident_t &newId);
    nodeident_t writeBuilderToPage(const nodeident_t &newId);
    //maybe_blockid findNextBlock(const bruce_fb::InternalNode *);
};

}

#endif
