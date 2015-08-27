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
 *
 * LEAF NODE
 * ---------
 *   [ uint16 ]           N of KV-pairs
 *   [ N x ... bytes ]    all keys, type-specific serialization
 *   [ N x ... bytes ]    all values, type-specific serialization
 *
 * INTERNAL NODE
 * -------------
 *   [ uint16 ]           N of node pointers
 *   [ N-1 x ... bytes ]  keys s.t. max_key(leaf(i)) < key(i) <= min_key(leaf(i+1))
 *   [ N x uint64 ]       node identifiers
 *   [ N x uint32 ]       item counts per node
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

#include <libbruce/memory.h>
#include <libbruce/types.h>

#include "nodes.h"

namespace bruce {

// Sizes of types inside the block
typedef uint16_t flags_t;

node_ptr ParseNode(memory &input, const tree_functions &fns);

memory SerializeNode(const node_ptr &node);


struct NodeSize
{
    bool shouldSplit();
    keycount_t splitIndex();
    uint32_t size() const { return m_size; }
protected:
    NodeSize(uint32_t blockSize);

    uint32_t m_blockSize;
    uint32_t m_size;
    keycount_t m_splitIndex;
};


struct LeafNodeSize : public NodeSize
{
    LeafNodeSize(const leafnode_ptr &node, uint32_t blockSize);
};

struct InternalNodeSize : public NodeSize
{
    InternalNodeSize(const internalnode_ptr &node, uint32_t blockSize);
};


}

#endif
