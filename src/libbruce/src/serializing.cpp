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

NodeReader::NodeReader(const range &input, fn::sizeinator *keySizeFn, fn::sizeinator *valueSizeFn)
    : m_input(input), m_keySizeFn(keySizeFn), m_valueSizeFn(valueSizeFn)
{
    // Skip flags and count header
    m_offset = sizeof(flags_t) + sizeof(keycount_t);

    if (input.size() < m_offset)
        throw std::runtime_error("Node data too small to parse");

    if (isLeafNode())
    {
        m_k_offsets.reserve(count());
        m_v_offsets.reserve(count());
        findLeafOffsets();
    }
    else
    {
        m_k_offsets.reserve(count() - 1);
        m_v_offsets.reserve(count());
        m_c_offsets.reserve(count());
        findInternalOffsets();
    }
}

bool NodeReader::isLeafNode() const
{
    return (flags() & TYPE_INTERNAL) == 0;
}

flags_t NodeReader::flags() const
{
    return *(flags_t*)m_input.ptr();
}

void NodeReader::validateOffset()
{
    if (m_offset >= m_input.size())
        throw std::runtime_error("Unexpected end of block while parsing");
}

void NodeReader::findLeafOffsets()
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

void NodeReader::findInternalOffsets()
{
    // Read N-1 keys, starting at 1
    m_k_offsets.push_back(range());
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
        m_v_offsets.push_back(m_input.slice(m_offset, size));
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

range NodeReader::key(keycount_t i) const
{
    return m_k_offsets[i];
}

range NodeReader::value(keycount_t i) const
{
    if (!isLeafNode()) throw std::runtime_error("Values only in leaves");
    return m_v_offsets[i];
}

nodeident_t NodeReader::id(keycount_t i) const
{
    if (isLeafNode()) throw std::runtime_error("IDs only in internal nodes");
    return *m_v_offsets[i].at<nodeident_t>(0);
}

itemcount_t NodeReader::itemCount(keycount_t i) const
{
    if (isLeafNode()) throw std::runtime_error("Counts only in internal nodes");
    return *m_c_offsets[i].at<itemcount_t>(0);
}

//----------------------------------------------------------------------

void LeafNodeWriter::writePair(const range &key, const range &value)
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

range LeafNodeWriter::get() const
{
    uint32_t s = size();

    range mem(range::memptr(new char[s]), s);

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

void InternalNodeWriter::writeNode(const range &min_key, nodeident_t id, itemcount_t count)
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

range InternalNodeWriter::get() const
{
    uint32_t s = size();

    range mem(range::memptr(new char[s]), s);

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
