#pragma once
#ifndef BSTAT_STATS_H
#define BSTAT_STATS_H

#include <vector>
#include <iostream>
#include "visitor.h"


typedef std::vector<int> intlist_t;
typedef std::vector<libbruce::nodeid_t> idlist_t;

struct StatsCollector : public BruceVisitor
{
    StatsCollector(size_t blockSize, size_t queueSize);
    ~StatsCollector();

    void visitInternal(const libbruce::nodeid_t &id, const libbruce::internalnode_ptr &node, int depth);
    void visitLeaf(const libbruce::nodeid_t &id, const libbruce::leafnode_ptr &leaf, int depth);
    void visitOverflow(const libbruce::nodeid_t &id, const libbruce::overflownode_ptr &overflow, int depth);

    void print(std::ostream &os);

private:
    size_t m_blockSize;
    size_t m_queueSize;
    size_t m_internalBlockSize;

    size_t m_leafValues;
    size_t m_overflowValues;
    size_t m_intBranches;
    size_t m_queuedEdits;

    idlist_t m_ids;
    intlist_t m_leafSizes;
    intlist_t m_internalSizes;
    intlist_t m_overflowSizes;
    intlist_t m_queueSizes;

    int m_maxLeafDepth;
    int m_maxDepth;
    int m_longestOverflowChain;
    int m_lastLeafDepth;
};

#endif
