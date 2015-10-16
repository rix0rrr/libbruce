#pragma once
#ifndef VISITOR_H
#define VISITOR_H

#include <libbruce/bruce.h>

// Using private API here, nasty
#include "../libbruce/src/nodes.h"

/**
 * Visitor interface
 */
struct BruceVisitor
{
    virtual ~BruceVisitor();

    virtual void visitInternal(const libbruce::internalnode_ptr &node) = 0;
    virtual void visitLeaf(const libbruce::leafnode_ptr &leaf) = 0;
    virtual void visitOverflow(const libbruce::overflownode_ptr &overflow) = 0;
};

void walk(const libbruce::nodeid_t &id, BruceVisitor &visitor, libbruce::be::be &blockEngine, const libbruce::tree_functions &fns);

#endif
