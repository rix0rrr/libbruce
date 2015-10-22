#pragma once
#ifndef BRUCE_SERIALIZING_H
#define BRUCE_SERIALIZING_H

/**
 * Serialization/deserialization routines for nodes
 *
 * In principle, we try to be as 0-copy and as tight as possible. Serialization
 * may be type-specific though, so we need function pointers.
 *
 * Nodes are laid out with similar types next to each other so we get
 * compression gains.
 *
 * A serialized node looks like this:
 *
 *   [ uint16 ]  flags
 *        0x0000   leaf node
 *        0x0001   internal node
 *        0x0002   overflow node
 *
 * The invariant for overflow blocks is that the keys in there must ALL be the
 * same as the final key in the leaf block. In other words, we never split a key
 * across leaf nodes.
 *
 * LEAF NODE
 * ---------
 *   [ uint16 ]           flags
 *   [ uint32 ]           N of KV-pairs
 *   [ N x ... bytes ]    all keys, type-specific serialization
 *   [ N x ... bytes ]    all values, type-specific serialization
 *   [ uint32 ]           overflow node count
 *   [ hash160 ]          overflow node identifier
 *
 * OVERFLOW NODE
 * -------------
 *   [ uint16 ]           flags
 *   [ uint32 ]           N of values
 *   [ N x ... bytes ]    all values
 *   [ uint32 ]           next overflow node count
 *   [ hash160 ]          next overflow node ID
 *
 * INTERNAL NODE
 * -------------
 *   [ uint16 ]           flags
 *   [ uint32 ]           N of node pointers
 *   [ uint32 ]           M of queued edits
 *   [ N-1 x ... bytes ]  keys s.t. max_key(leaf(i)) < key(i) <= min_key(leaf(i+1))
 *   [ N x hash160 ]      node identifiers
 *   [ N x uint32 ]       item counts per node
 *   [ M x uint8 ]        types of queued edits
 *   [ M x ... bytes ]    keys of queued edits
 *   [ M x ... bytes ]    values of queued edits
 *
 * The structure ought to be read as follows:
 *
 *   internal node = .|.|.|.|.
 *
 * Where . = a pointer to another node
 *       | = a key that divides the keyspace for the node s.t. the key is >
 *       than the largest key in the left node and <= the smallest key in the
 *       right node.
 */

#include <stdint.h>
#include <vector>

#include <boost/make_shared.hpp>

#include <libbruce/mempage.h>
#include <libbruce/types.h>

#include "nodes.h"
#include "leaf_node.h"
#include "internal_node.h"
#include "overflow_node.h"

namespace libbruce {

// Sizes of types inside the block
typedef uint16_t flags_t;

node_ptr ParseNode(mempage &input, const tree_functions &fns);

mempage SerializeNode(const node_ptr &node);


struct NodeSize
{
    uint32_t size() const { return m_size; }
    bool shouldSplit();
protected:
    NodeSize(uint32_t blockSize);

    uint32_t m_blockSize;
    uint32_t m_size;
};


/**
 * Splits a leaf into potentially 3 blocks
 *
 * [0..overflowStart)           Contents of the left leaf
 * [overflowStart..splitIndex)  Contents of the overflow block of the left leaf
 * [splitIndex..N)              Contents of the right leaf
 */
struct LeafNodeSize : public NodeSize
{
    LeafNodeSize(const leafnode_ptr &node, uint32_t blockSize);

    pairlist_t::const_iterator splitStart() const { return m_splitStart; }
    pairlist_t::const_iterator overflowStart() const { return m_overflowStart; }
private:
    pairlist_t::const_iterator m_splitStart;
    pairlist_t::const_iterator m_overflowStart;
};

/**
 * The splitindex will point at the first value that exceeds the block size
 */
struct OverflowNodeSize : public NodeSize
{
    OverflowNodeSize(const overflownode_ptr &node, uint32_t blockSize);

    keycount_t splitIndex() const { return m_splitIndex; }
private:
    keycount_t m_splitIndex;
};

/**
 * The splitindex will point at the first branch that makes all branches below
 * them exceed half of the block size.
 */
struct InternalNodeSize : public NodeSize
{
    InternalNodeSize(const internalnode_ptr &node, uint32_t blockSize, uint32_t maxEditQueueSize);

    keycount_t splitIndex() const { return m_splitIndex; }
    bool shouldApplyEditQueue() const;
private:
    uint32_t m_maxEditQueueSize;
    uint32_t m_editQueueSize;
    keycount_t m_splitIndex;
};


}

#endif
