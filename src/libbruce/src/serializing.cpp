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

#define TYPE_INTERNAL 0x0001

namespace bruce {

Node::Node(const memory &input, fn::sizeinator *keySizeFn)
    : m_input(input), m_keySizeFn(keySizeFn), m_offset(sizeof(flags_t) + sizeof(keycount_t))
{
    if (m_input.size() < m_offset)
        throw std::runtime_error("Node data too small to parse");
}

Node::~Node()
{
}

void Node::validateOffset()
{
    if (m_offset >= m_input.size())
        throw std::runtime_error("Unexpected end of block while parsing");
}

//----------------------------------------------------------------------

node_ptr ParseNode(const memory &input, fn::sizeinator *keySizeFn, fn::sizeinator *valueSizeFn)
{
    if (*input.at<flags_t>(0) & TYPE_INTERNAL)
        return boost::make_shared<InternalNode>(input, keySizeFn);
    else
        return boost::make_shared<LeafNode>(input, keySizeFn, valueSizeFn);
}

//----------------------------------------------------------------------

LeafNode::LeafNode(const memory &input, fn::sizeinator *keySizeFn, fn::sizeinator *valueSizeFn)
    : Node(input, keySizeFn), m_valueSizeFn(valueSizeFn)
{
    m_k_offsets.reserve(count());
    m_v_offsets.reserve(count());
    parse();
}

bool LeafNode::isLeafNode() const
{
    return true;
}

memory LeafNode::value(keycount_t i) const
{
    return m_v_offsets[i];
}

void LeafNode::parse()
{
    // Read N keys
    for (keycount_t i = 0; i < count(); i++)
    {
        validateOffset();
        uint32_t size = m_keySizeFn(m_input.at<const char>(m_offset));
        m_k_offsets.push_back(m_input.slice(m_offset, size));
        m_offset += size;
    }

    // Read N values
    for (keycount_t i = 0; i < count(); i++)
    {
        validateOffset();
        uint32_t size = m_valueSizeFn(m_input.at<const char>(m_offset));
        m_v_offsets.push_back(m_input.slice(m_offset, size));
        m_offset += size;
    }

    if (m_offset > m_input.size())
        throw std::runtime_error("Last item truncated");
}

//----------------------------------------------------------------------

InternalNode::InternalNode(const memory &input, fn::sizeinator *keySizeFn)
    : Node(input, keySizeFn)
{
    m_k_offsets.reserve(count());
    m_i_offsets.reserve(count());
    m_c_offsets.reserve(count());
    parse();
}

bool InternalNode::isLeafNode() const
{
    return false;
}

nodeident_t InternalNode::id(keycount_t i) const
{
    return *m_i_offsets[i].at<nodeident_t>(0);
}

itemcount_t InternalNode::itemCount(keycount_t i) const
{
    return *m_c_offsets[i].at<itemcount_t>(0);
}

void InternalNode::parse()
{
    // Read N-1 keys, starting at 1
    m_k_offsets.push_back(memory());
    for (keycount_t i = 1; i < count(); i++)
    {
        validateOffset();
        uint32_t size = m_keySizeFn(m_input.at<const char>(m_offset));
        m_k_offsets.push_back(m_input.slice(m_offset, size));
        m_offset += size;
    }

    // Read N node identifiers
    for (keycount_t i = 0; i < count(); i++)
    {
        validateOffset();
        uint32_t size = sizeof(nodeident_t);
        m_i_offsets.push_back(m_input.slice(m_offset, size));
        m_offset += size;
    }

    // Read N item counts
    for (keycount_t i = 0; i < count(); i++)
    {
        validateOffset();
        uint32_t size = sizeof(itemcount_t);
        m_c_offsets.push_back(m_input.slice(m_offset, size));
        m_offset += size;
    }

    if (m_offset > m_input.size())
        throw std::runtime_error("Last item truncated");
}

//----------------------------------------------------------------------

void LeafNodeWriter::writePair(const memory &key, const memory &value)
{
    m_keys.push_back(key);
    m_values.push_back(value);
}

uint32_t LeafNodeWriter::size() const
{
    uint32_t size = sizeof(flags_t) + sizeof(keycount_t);
    for (keycount_t i = 0; i <  m_keys.size(); i++)
    {
        size += m_keys[i].size() + m_values[i].size();
    }
    return size;
}

memory LeafNodeWriter::get() const
{
    uint32_t s = size();

    memory mem(memory::memptr(new char[s]), s);

    uint32_t offset = 0;

    // Flags
    *mem.at<flags_t>(offset) = 0;
    offset += sizeof(flags_t);

    // Count
    *mem.at<keycount_t>(offset) = m_keys.size();
    offset += sizeof(keycount_t);

    // Keys
    for (keycount_t i = 0; i < m_keys.size(); i++)
    {
        memcpy(mem.at<char>(offset), m_keys[i].ptr(), m_keys[i].size());
        offset += m_keys[i].size();
    }

    // Values
    for (keycount_t i = 0; i < m_keys.size(); i++)
    {
        memcpy(mem.at<char>(offset), m_values[i].ptr(), m_values[i].size());
        offset += m_values[i].size();
    }

    return mem;
}

//----------------------------------------------------------------------

void InternalNodeWriter::writeNode(const memory &min_key, nodeident_t id, itemcount_t count)
{
    m_keys.push_back(min_key);
    m_ids.push_back(id);
    m_counts.push_back(count);
}

uint32_t InternalNodeWriter::size() const
{
    uint32_t size = sizeof(flags_t) + sizeof(keycount_t);
    for (keycount_t i = 0; i <  m_keys.size(); i++)
    {
        if (i > 0)
            size += m_keys[i].size();

        size += sizeof(m_ids[i]) + sizeof(m_counts[i]);
    }
    return size;
}

memory InternalNodeWriter::get() const
{
    uint32_t s = size();

    memory mem(memory::memptr(new char[s]), s);

    uint32_t offset = 0;

    // Flags
    *mem.at<flags_t>(offset) = TYPE_INTERNAL;
    offset += sizeof(flags_t);

    // Count
    *mem.at<keycount_t>(offset) = m_keys.size();
    offset += sizeof(keycount_t);

    // Keys (except the 1st one)
    for (keycount_t i = 1; i < m_keys.size(); i++)
    {
        memcpy(mem.at<char>(offset), m_keys[i].ptr(), m_keys[i].size());
        offset += m_keys[i].size();
    }

    // IDs (all at once)
    memcpy(mem.at<char>(offset), m_ids.data(), m_ids.size() * sizeof(m_ids[0]));
    offset += m_ids.size() * sizeof(m_ids[0]);

    // Item counts (all at once)
    memcpy(mem.at<char>(offset), m_counts.data(), m_counts.size() * sizeof(m_counts[0]));

    return mem;
}

}
