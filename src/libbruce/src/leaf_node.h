#pragma once
#ifndef LEAF_NODE_H
#define LEAF_NODE_H

#include "nodes.h"

#include <algorithm>
#include <boost/container/container_fwd.hpp>
#include <boost/container/flat_map.hpp>

namespace libbruce {

typedef std::pair<memslice, memslice> kv_pair;

// Flat sorted vector
typedef boost::container::vector<kv_pair> pairlist_t;

struct PairOrder : public KeyOrder
{
    PairOrder(const tree_functions &fns) : KeyOrder(fns) { }

    bool operator()(const memslice &a, const kv_pair &b) const { return KeyOrder::operator()(a, b.first); }
    bool operator()(const kv_pair &a, const memslice &b) const { return KeyOrder::operator()(a.first, b); }
};

/**
 * Leaf node type
 */
struct LeafNode : public Node
{
    LeafNode(const tree_functions &fns);
    LeafNode(std::vector<kv_pair>::const_iterator begin, std::vector<kv_pair>::const_iterator end, const tree_functions &fns);
    LeafNode(pairlist_t::const_iterator begin, pairlist_t::const_iterator end, const tree_functions &fns);

    keycount_t pairCount() const { return pairs.size(); }
    virtual const memslice &minKey() const;
    virtual itemcount_t itemCount() const;

    void insert(const kv_pair &item)
    {
        pairlist_t::iterator it = std::upper_bound(pairs.begin(), pairs.end(), item.first, m_order);
        pairs.insert(it, item);
        m_elementsSize += item.first.size() + item.second.size();
    }

    pairlist_t::iterator erase(const pairlist_t::iterator &it)
    {
        m_elementsSize -= it->first.size() + it->second.size();
        return pairs.erase(it);
    }

    void update_value(pairlist_t::iterator &it, const memslice &value)
    {
        m_elementsSize -= it->second.size();
        m_elementsSize += value.size();
        it->second = value;
    }

    pairlist_t::iterator find(const memslice &key)
    {
        pairlist_t::iterator it = std::lower_bound(pairs.begin(), pairs.end(), key, m_order);
        if (it != pairs.end() && key == it->first) return it;
        return pairs.end();
    }

    // Return a value by index (slow, only for testing!)
    pairlist_t::const_iterator get_at(int n) const;
    pairlist_t::iterator get_at(int n);

    void setOverflow(const node_ptr &node);

    pairlist_t pairs;
    overflow_t overflow;

    size_t elementsSize() const { return m_elementsSize; }

    void print(std::ostream &os) const;
    void findRange(const memslice &key, pairlist_t::iterator *begin, pairlist_t::iterator *end);

private:
    PairOrder m_order;
    size_t m_elementsSize;
    void calcSize();
};


}

#endif
