#ifndef BRUCE_SERIALIZING_H
#define BRUCE_SERIALIZING_H

/**
 * Serialization/deserialization routines for blocks
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

#include <libbruce/memory.h>
#include <stdint.h>
#include <vector>

namespace bruce {

namespace fn {
typedef uint32_t sizeinator(const void *);
}

// Sizes of types inside the block
typedef uint16_t flags_t;
typedef uint16_t keycount_t;
typedef uint64_t nodeident_t;
typedef uint32_t itemcount_t;

/**
 * Parse a node's serialization
 *
 * As 0-copy as possible, only one iteration is used during parsing to
 * establish offsets, after that we index into the block.
 */
class NodeReader {
public:
    NodeReader(const range &input, fn::sizeinator *keySizeFn, fn::sizeinator *valueSizeFn);

    bool isLeafNode() const;

    keycount_t count() const { return *(keycount_t*)(m_input.byte_ptr() + sizeof(flags_t)); }

    // For both node types. For internal nodes, the first key is empty.
    range key(keycount_t i) const;

    // For leaf types
    range value(keycount_t i) const;

    // For internal types
    nodeident_t id(keycount_t i) const;
    itemcount_t itemCount(keycount_t i) const;
private:
    range m_input;
    fn::sizeinator *m_keySizeFn;
    fn::sizeinator *m_valueSizeFn;
    uint32_t m_offset; // Used while parsing

    std::vector<range> m_k_offsets; // Key offsets, [0..count) for leaves, [0..count-1) for internals
    std::vector<range> m_v_offsets; // Value offsets for leaves, identifier offsets for internals
    std::vector<range> m_c_offsets; // Count offsets for internals

    flags_t flags() const;
    void findLeafOffsets();
    void findInternalOffsets();
    void validateOffset();
};

/**
 * Serialize an internal node
 *
 * Serializing involves a little more copying, under the assumption it's not
 * done as often. We keep copies of the data in memory until we can copy it all
 * into a block.
 *
 * We don't write the leftmost key into the target block, it's there only
 * for a uniform API.
 */
class InternalNodeWriter
{
public:
    void writeNode(const range &min_key, nodeident_t id, itemcount_t count);
    range get() const;
private:
    std::vector<range> m_keys;
    std::vector<nodeident_t> m_ids;
    std::vector<itemcount_t> m_counts;

    uint32_t size() const;
};

/**
 * Serialize a leaf node.
 *
 * Serializing involves a little more copying, under the assumption it's not
 * done as often. We keep copies of the data in memory until we can copy it all
 * into a block.
 */
class LeafNodeWriter
{
public:
    void writePair(const range &key, const range &value);
    range get() const;
private:
    std::vector<range> m_keys;
    std::vector<range> m_values;

    uint32_t size() const;
};


}

#endif
