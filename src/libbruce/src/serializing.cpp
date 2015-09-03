/**
 * IMPLEMENTATION NOTES
 *
 * Parsing an incomplete block with the sizeinator may go off the end of the
 * block. However, any reasonable block size <16k will simply be allocated by
 * the memory manager, so we won't go off the page, and we'll detect the size
 * mismatch at the next item or at the end.
 *
 * Valgrind may complain though.
 */
#include "serializing.h"

#include <cmath>

#include <boost/foreach.hpp>

#define TYPE_INTERNAL 0x0001

namespace bruce {


struct NodeParser
{
    NodeParser(memory &input, const tree_functions &fns)
        : m_input(input), fns(fns), m_offset(sizeof(flags_t) + sizeof(keycount_t)) { }

    leafnode_ptr parseLeafNode()
    {
        leafnode_ptr ret = boost::make_shared<LeafNode>(keyCount());

        // Read N keys
        for (keycount_t i = 0; i < keyCount(); i++)
        {
            validateOffset();
            uint32_t size = fns.keySize(m_input.at<const char>(m_offset));

            // Push back
            ret->pairs().push_back(kv_pair(m_input.slice(m_offset, size), memory()));

            m_offset += size;
        }

        // Read N values
        for (keycount_t i = 0; i < keyCount(); i++)
        {
            validateOffset();
            uint32_t size = fns.valueSize(m_input.at<const char>(m_offset));

            // Couple value to key
            ret->pairs()[i].value = m_input.slice(m_offset, size);

            m_offset += size;
        }

        validateNotPastEnd();

        return ret;
    }

    internalnode_ptr parseInternalNode()
    {
        internalnode_ptr ret = boost::make_shared<InternalNode>(keyCount());

        // Read N-1 keys, starting at 1
        ret->branches().push_back(node_branch(memory(), 0, 0));
        for (keycount_t i = 1; i < keyCount(); i++)
        {
            validateOffset();
            uint32_t size = fns.keySize(m_input.at<const char>(m_offset));

            ret->branches().push_back(node_branch(m_input.slice(m_offset, size), 0, 0));

            m_offset += size;
        }

        // Read N node identifiers
        for (keycount_t i = 0; i < keyCount(); i++)
        {
            validateOffset();
            uint32_t size = sizeof(nodeid_t);

            ret->branches()[i].nodeID = *m_input.at<nodeid_t>(m_offset);

            m_offset += size;
        }

        // Read N item counts
        for (keycount_t i = 0; i < keyCount(); i++)
        {
            validateOffset();
            uint32_t size = sizeof(itemcount_t);

            ret->branches()[i].itemCount = *m_input.at<itemcount_t>(m_offset);

            m_offset += size;
        }

        validateNotPastEnd();

        return ret;
    }

private:
    keycount_t keyCount()
    {
        return *m_input.at<keycount_t>(sizeof(flags_t));
    }

    void validateOffset()
    {
        if (m_offset >= m_input.size())
            throw std::runtime_error("End of block while parsing node data");
    }

    void validateNotPastEnd()
    {
        if (m_offset > m_input.size())
            throw std::runtime_error("Last item truncated");
    }

    size_t m_offset;
    memory &m_input;
    const tree_functions &fns;
};

//----------------------------------------------------------------------

node_ptr ParseInternalNode(memory &input, const tree_functions &fns)
{
    NodeParser parser(input, fns);
    return parser.parseInternalNode();
}

node_ptr ParseLeafNode(memory &input, const tree_functions &fns)
{
    NodeParser parser(input, fns);
    return parser.parseLeafNode();
}

node_ptr ParseNode(memory &input, const tree_functions &fns)
{
    node_ptr ret;
    if (*input.at<flags_t>(0) & TYPE_INTERNAL)
        ret = ParseInternalNode(input, fns);
    else
        ret = ParseLeafNode(input, fns);
    ret->clean();
    return ret;
}

//----------------------------------------------------------------------

NodeSize::NodeSize(uint32_t blockSize)
    : m_blockSize(blockSize), m_size(sizeof(flags_t) + sizeof(keycount_t))
{
}

bool NodeSize::shouldSplit()
{
    return m_blockSize < m_size;
}

keycount_t NodeSize::splitIndex()
{
    return m_splitIndex;
}

LeafNodeSize::LeafNodeSize(const leafnode_ptr &node, uint32_t blockSize)
    : NodeSize(blockSize)
{
    uint32_t splitSize = m_size;

    // Calculate size
    BOOST_FOREACH(const kv_pair &p, node->pairs())
    {
        m_size += p.key.size() + p.value.size();
    }

    if (shouldSplit())
    {
        uint32_t pieceSize = std::ceil(m_size / 2.0);

        for (m_splitIndex = 1; m_splitIndex < node->count(); m_splitIndex++)
        {
            splitSize += node->pair(m_splitIndex-1).key.size() + node->pair(m_splitIndex-1).value.size();
            if (splitSize >= pieceSize)
                return;
        }
    }
}

InternalNodeSize::InternalNodeSize(const internalnode_ptr &node, uint32_t blockSize)
    : NodeSize(blockSize)
{
    uint32_t splitSize = m_size;
    bool first = true;

    BOOST_FOREACH(const node_branch &b, node->branches())
    {
        // We never store the first key
        if (!first)
            m_size += b.minKey.size();
        m_size += sizeof(nodeid_t) + sizeof(itemcount_t);
        first = false;
    }

    if (shouldSplit())
    {
        uint32_t pieceSize = std::ceil(m_size / 2.0);

        for (m_splitIndex = 1; m_splitIndex < node->count(); m_splitIndex++)
        {
            if (m_splitIndex != 1) m_size += node->branch(m_splitIndex-1).minKey.size();
            splitSize += sizeof(nodeid_t) + sizeof(itemcount_t);
            first = false;

            if (splitSize >= pieceSize)
                return;
        }
    }
}

//----------------------------------------------------------------------

memory SerializeLeafNode(const leafnode_ptr &node)
{
    LeafNodeSize size(node, 0);
    memory mem(memory::memptr(new char[size.size()]), size.size());

    uint32_t offset = 0;

    // Flags
    *mem.at<flags_t>(offset) = 0;
    offset += sizeof(flags_t);

    // Count
    *mem.at<keycount_t>(offset) = node->count();
    offset += sizeof(keycount_t);

    // Keys
    BOOST_FOREACH(const kv_pair &pair, node->pairs())
    {
        memcpy(mem.at<char>(offset), pair.key.ptr(), pair.key.size());
        offset += pair.key.size();
    }

    // Values
    BOOST_FOREACH(const kv_pair &pair, node->pairs())
    {
        memcpy(mem.at<char>(offset), pair.value.ptr(), pair.value.size());
        offset += pair.value.size();
    }

    return mem;
}

memory SerializeInternalNode(const internalnode_ptr &node)
{
    InternalNodeSize size(node, 0);
    memory mem(memory::memptr(new char[size.size()]), size.size());

    uint32_t offset = 0;

    // Flags
    *mem.at<flags_t>(offset) = TYPE_INTERNAL;
    offset += sizeof(flags_t);

    // Count
    *mem.at<keycount_t>(offset) = node->count();
    offset += sizeof(keycount_t);

    // Keys (except the 1st one)
    bool first = true;
    BOOST_FOREACH(const node_branch &b, node->branches())
    {
        if (!first)
        {
            memcpy(mem.at<char>(offset), b.minKey.ptr(), b.minKey.size());
            offset += b.minKey.size();
        }
        first = false;
    }

    // IDs
    BOOST_FOREACH(const node_branch &b, node->branches())
    {
        *mem.at<nodeid_t>(offset) = b.nodeID;
        offset += sizeof(nodeid_t);
    }

    // Item counts
    BOOST_FOREACH(const node_branch &b, node->branches())
    {
        *mem.at<itemcount_t>(offset) = b.itemCount;
        offset += sizeof(itemcount_t);
    }

    return mem;
}

memory SerializeNode(const node_ptr &node)
{
    if (node->isLeafNode())
        return SerializeLeafNode(boost::static_pointer_cast<LeafNode>(node));
    else
        return SerializeInternalNode(boost::static_pointer_cast<InternalNode>(node));
}

}
