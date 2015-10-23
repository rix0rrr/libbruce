#pragma once
#ifndef LEAF_NODE_H
#define LEAF_NODE_H

#include "nodes.h"

#include <boost/container/container_fwd.hpp>
#include <boost/container/flat_map.hpp>

namespace libbruce {

typedef std::pair<memslice, memslice> kv_pair;

// Flat multimap gives the best performance
typedef boost::container::flat_multimap<memslice, memslice, KeyOrder> pairlist_t;

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
        m_elementsSize += item.first.size() + item.second.size();
        pairs.insert(item);
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
    size_t m_elementsSize;
    void calcSize();
};


}

#endif
