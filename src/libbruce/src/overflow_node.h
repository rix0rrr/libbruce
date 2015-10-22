#pragma once
#ifndef OVERFLOW_NODE_H
#define OVERFLOW_NODE_H

#include "nodes.h"
#include "leaf_node.h"

namespace libbruce {

typedef std::vector<memslice> valuelist_t;

/**
 * Overflow nodes contain lists of values with the same key as the last key in a leaf
 */
struct OverflowNode : public Node
{
    OverflowNode(size_t sizeHint=0);
    OverflowNode(pairlist_t::const_iterator begin, pairlist_t::const_iterator end);
    OverflowNode(valuelist_t::const_iterator begin, valuelist_t::const_iterator end);

    virtual itemcount_t itemCount() const;
    virtual const memslice &minKey() const;

    keycount_t valueCount() const { return values.size(); }

    void append(const memslice &item) { values.push_back(item); }
    void erase(size_t i) { values.erase(values.begin() + i); }

    valuelist_t::const_iterator at(keycount_t i) const { return values.begin() + i; }
    valuelist_t::iterator at(keycount_t i) { return values.begin() + i; }

    void setNext(const node_ptr &node);
    void print(std::ostream &os) const;

    valuelist_t values;
    overflow_t next;
};


}

#endif
