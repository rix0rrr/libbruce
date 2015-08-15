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

#include <libbruce/memory.h>
#include <stdint.h>
#include <vector>

#include <boost/make_shared.hpp>

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
 * Base class for Nodes
 */
struct Node
{
    virtual ~Node();

    virtual bool isLeafNode() const = 0;

    range key(keycount_t i) const { return m_k_offsets[i]; }
    keycount_t count() const { return *(keycount_t*)(m_input.byte_ptr() + sizeof(flags_t)); }
protected:
    Node(const range &input, fn::sizeinator *keySizeFn);

    range m_input;
    fn::sizeinator *m_keySizeFn;
    uint32_t m_offset; // Used while parsing in the children

    std::vector<range> m_k_offsets; // Key offsets (leftmost key is worthless for internal nodes)

    void validateOffset();
};

/**
 * Leaf node type
 */
struct LeafNode : public Node
{
    LeafNode(const range &input, fn::sizeinator *keySizeFn, fn::sizeinator *valueSizeFn);

    bool isLeafNode() const;
    range value(keycount_t i) const;

    keycount_t count() const { return *(keycount_t*)(m_input.byte_ptr() + sizeof(flags_t)); }
private:
    fn::sizeinator *m_valueSizeFn;
    std::vector<range> m_v_offsets; // Value offsets for leaves, identifier offsets for internals

    void parse();
};

/**
 * Internal node type
 */
struct InternalNode : public Node
{
    InternalNode(const range &input, fn::sizeinator *keySizeFn);

    bool isLeafNode() const;

    nodeident_t id(keycount_t i) const;
    itemcount_t itemCount(keycount_t i) const;
private:
    std::vector<range> m_i_offsets;
    std::vector<range> m_c_offsets;

    void parse();
};

typedef boost::shared_ptr<Node> node_ptr;
typedef boost::shared_ptr<LeafNode> leafnode_ptr;
typedef boost::shared_ptr<InternalNode> internalnode_ptr;

node_ptr ParseNode(const range &input, fn::sizeinator *keySizeFn, fn::sizeinator *valueSizeFn);

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
