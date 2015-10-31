#include "stats.h"
#include "table.h"

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

StatsCollector::StatsCollector(size_t blockSize, size_t queueSize)
    : m_blockSize(blockSize)
    , m_queueSize(queueSize)
    , m_internalBlockSize(blockSize - queueSize)
    , m_leafValues(0)
    , m_overflowValues(0)
    , m_intBranches(0)
    , m_queuedEdits(0)
    , m_maxLeafDepth(0)
    , m_maxDepth(0)
    , m_longestOverflowChain(0)
{
}

StatsCollector::~StatsCollector()
{
}

void StatsCollector::visitInternal(const nodeid_t &id, const internalnode_ptr &node, int depth)
{
    m_ids.push_back(id);
    m_maxDepth = std::max(depth, m_maxDepth);

    InternalNodeSize s(node, 0, 0);
    m_internalSizes.push_back(s.size());

    m_intBranches += node->branches.size();
    m_queuedEdits += node->editQueue.size();
    m_queueSizes.push_back(s.editQueueSize());
}

void StatsCollector::visitLeaf(const nodeid_t &id, const leafnode_ptr &leaf, int depth)
{
    m_ids.push_back(id);
    m_maxDepth = std::max(depth, m_maxDepth);
    m_maxLeafDepth = std::max(depth, m_maxLeafDepth);

    LeafNodeSize s(leaf, 0);
    m_leafSizes.push_back(s.size());

    m_leafValues += leaf->pairs.size();

    m_lastLeafDepth = depth;
}

void StatsCollector::visitOverflow(const nodeid_t &id, const overflownode_ptr &overflow, int depth)
{
    m_ids.push_back(id);
    m_maxDepth = std::max(depth, m_maxDepth);
    m_longestOverflowChain = std::max(depth - m_lastLeafDepth, m_longestOverflowChain);

    OverflowNodeSize s(overflow, 0);
    m_overflowSizes.push_back(s.size());

    m_overflowValues += overflow->values.size();
}

std::string human_bytes(size_t s)
{
    char buffer[20];

    if (s > 1024 * 1024)
        sprintf(buffer, "%.2fM", (double)s / (1024 * 1024));
    else if (s > 1024)
        sprintf(buffer, "%.2fk", (double)s / 1024);
    else
        sprintf(buffer, "%lub", s);

    return std::string(buffer);
}

double capacity(double branchDegree, double leafDegree, int depth)
{
    return leafDegree * pow(branchDegree, depth - 1);
}

void StatsCollector::print(std::ostream &os)
{
    int nodeCount = m_leafSizes.size() + m_overflowSizes.size() + m_internalSizes.size();
    size_t totalSize = sum(m_leafSizes) + sum(m_overflowSizes) + sum(m_internalSizes);

    // Node list
    os << "Node list:" << std::endl;
    for (idlist_t::const_iterator it = m_ids.begin(); it != m_ids.end(); ++it)
        os << *it << std::endl;

    os  << std::endl;

    // Stats
    Table t;

    t.put("")
        .put("Leaf")
        .put("Overflow")
        .put("Internal")
        .put("Total")
        .newline();

    t.put("")
        .put("-------")
        .put("-------")
        .put("-------")
        .put("-------")
        .newline();

    t.put("Node Counts")
        .put(m_leafSizes.size())
        .put(m_overflowSizes.size())
        .put(m_internalSizes.size())
        .put(nodeCount)
        .newline();

    t.put("Values")
        .put(m_leafValues)
        .put(m_overflowValues)
        .put("")
        .put(m_leafValues + m_overflowValues)
        .newline();

    t.put("Branches")
        .put("")
        .put("")
        .put(m_intBranches)
        .put("")
        .newline();

    t.put("Queued edits")
        .put("")
        .put("")
        .put(m_queuedEdits)
        .newline();

    t.put("Value Sizes")
        .put(human_bytes(sum(m_leafSizes)))
        .put(human_bytes(sum(m_overflowSizes)))
        .put(human_bytes(sum(m_internalSizes)))
        .put(human_bytes(totalSize))
        .newline();

    t.put("Queued edit sizes")
        .put("")
        .put("")
        .put(human_bytes(sum(m_queueSizes)))
        .newline();

    t.put("Fill degree")
        .perc(((double)sum(m_leafSizes) / (m_leafSizes.size() * m_blockSize)))
        .perc(((double)sum(m_overflowSizes) / (m_overflowSizes.size() * m_blockSize)))
        .perc(((double)sum(m_internalSizes) / (m_internalSizes.size() * m_internalBlockSize)))
        .perc(((double)totalSize / (nodeCount * m_blockSize)))
        .newline();

    t.put("Tree depth")
        .put(m_maxLeafDepth)
        .put("")
        .put("")
        .put(m_maxDepth)
        .newline();

    t.put("Max overflow chain")
        .put("")
        .put(m_longestOverflowChain)
        .put("")
        .put("")
        .newline();

    t.print(os);

    double avgBranchSize = (double)sum(m_internalSizes) / m_intBranches;
    int branchDegree = m_internalBlockSize / avgBranchSize;

    double avgValueSize = (double)sum(m_leafSizes) / m_leafValues;
    int leafDegree = m_blockSize / avgValueSize;

    Table()
        .put("Branch degree").put(branchDegree)
        .newline()
        .put("Leaf degree").put(leafDegree)
        .newline()
        .put("Estimated capacity").put("Depth 1").put(capacity(branchDegree, leafDegree, 1)).newline()
        .put("").put("Depth 2").put(capacity(branchDegree, leafDegree, 2)).newline()
        .put("").put("Depth 3").put(capacity(branchDegree, leafDegree, 3)).newline()
        .print(os);

    // Calculate rank
    // Calculate 2, 3, 4 level tree cardinalities
}
