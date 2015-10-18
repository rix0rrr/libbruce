#include "stats.h"

#include "../libbruce/src/serializing.h"

#include <algorithm>

using namespace libbruce;

int sum(const intlist_t &xs)
{
    int ret = 0;
    for (intlist_t::const_iterator it = xs.begin(); it != xs.end(); ++it)
    {
        ret += *it;
    }
    return ret;
}

StatsCollector::StatsCollector(size_t blockSize)
    : m_blockSize(blockSize)
    , m_leafValues(0)
    , m_overflowValues(0)
    , m_maxLeafDepth(0)
    , m_maxDepth(0)
    , m_longestOverflowChain(0)
{
}

StatsCollector::~StatsCollector()
{
}

void StatsCollector::visitInternal(const internalnode_ptr &node, int depth)
{
    m_maxDepth = std::max(depth, m_maxDepth);

    InternalNodeSize s(node, 0);
    m_internalSizes.push_back(s.size());
}

void StatsCollector::visitLeaf(const leafnode_ptr &leaf, int depth)
{
    m_maxDepth = std::max(depth, m_maxDepth);
    m_maxLeafDepth = std::max(depth, m_maxLeafDepth);

    LeafNodeSize s(leaf, 0);
    m_leafSizes.push_back(s.size());

    m_leafValues += leaf->pairs.size();

    m_lastLeafDepth = depth;
}

void StatsCollector::visitOverflow(const overflownode_ptr &overflow, int depth)
{
    m_maxDepth = std::max(depth, m_maxDepth);
    m_longestOverflowChain = std::max(depth - m_lastLeafDepth, m_longestOverflowChain);

    OverflowNodeSize s(overflow, 0);
    m_overflowSizes.push_back(s.size());

    m_overflowValues += overflow->values.size();
}

void StatsCollector::print(std::ostream &os)
{
    int nodeCount = m_leafSizes.size() + m_overflowSizes.size() + m_internalSizes.size();
    size_t totalSize = sum(m_leafSizes) + sum(m_overflowSizes) + sum(m_internalSizes);

    os  << "Leaf values                : " << m_leafValues << std::endl
        << "Overflow values            : " << m_overflowValues << std::endl
        << "Total values               : " << (m_leafValues + m_overflowValues) << std::endl
        << std::endl
        << "Leaf nodes                 : " << m_leafSizes.size() << std::endl
        << "Overflow nodes             : " << m_overflowSizes.size() << std::endl
        << "Internal nodes             : " << m_internalSizes.size() << std::endl
        << "Total nodes                : " << nodeCount << std::endl
        << std::endl
        << "Leaf size                  : " << sum(m_leafSizes) << std::endl
        << "Overflow size              : " << sum(m_overflowSizes) << std::endl
        << "Internal size              : " << sum(m_internalSizes) << std::endl
        << "Total size                 : " << totalSize << std::endl
        << "Fill degree                : " << ((double)(100 * totalSize) / (nodeCount * m_blockSize)) << "%" << std::endl
        << std::endl
        << "Depth                      : " << m_maxDepth << std::endl
        << "Leaf depth                 : " << m_maxLeafDepth << std::endl
        << "Longest overflow chain     : " << m_longestOverflowChain << std::endl;
}
