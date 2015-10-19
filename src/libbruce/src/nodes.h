#pragma once
#ifndef BRUCE_NODES_H
#define BRUCE_NODES_H

#include <utility>
#include <libbruce/memslice.h>
#include <libbruce/types.h>
#include <boost/make_shared.hpp>
#include <boost/container/container_fwd.hpp>
#include <boost/container/flat_map.hpp>

#include <vector>
#include <map>

namespace libbruce {

enum node_type_t {
    TYPE_LEAF,
    TYPE_INTERNAL,
    TYPE_OVERFLOW
};

class Node;

typedef boost::shared_ptr<Node> node_ptr;

typedef std::pair<memslice, memslice> kv_pair;
struct node_branch;

struct KeyOrder
{
    KeyOrder(const tree_functions &fns) : fns(fns) { }
    tree_functions fns;

    bool operator()(const memslice &a, const memslice &b) const
    {
        if (a.empty()) return true;
        if (b.empty()) return false;

        return fns.keyCompare(a, b) < 0;
    }
};

typedef boost::container::flat_multimap<memslice, memslice, KeyOrder> pairlist_t;

typedef std::vector<memslice> valuelist_t;

typedef std::vector<node_branch> branchlist_t;

struct overflow_t
{
    overflow_t() : count(0), nodeID() { }

    itemcount_t count;
    nodeid_t nodeID;
    node_ptr node; // Only valid while mutating the tree

    bool empty() const { return !count; }
};


/**
 * Base class for Nodes
 */
struct Node
{
    Node(node_type_t nodeType);
    virtual ~Node();

    virtual itemcount_t itemCount() const = 0; // Items in this node and below
    virtual const memslice &minKey() const = 0;

    node_type_t nodeType() const { return m_nodeType; }
private:
    node_type_t m_nodeType;
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
private:
    size_t m_elementsSize;
    void calcSize();
};

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

    valuelist_t values;
    overflow_t next;
};

/**
 * Internal node type
 */
struct InternalNode : public Node
{
    InternalNode(size_t sizeHint=0);
    InternalNode(branchlist_t::const_iterator begin, branchlist_t::const_iterator end);

    keycount_t branchCount() const { return branches.size(); }
    virtual const memslice &minKey() const;

    void insert(size_t i, const node_branch &branch) { branches.insert(branches.begin() + i, branch); }
    void erase(size_t i) { branches.erase(branches.begin() + i); }
    void append(const node_branch &branch) { branches.push_back(branch); }

    branchlist_t::const_iterator at(keycount_t i) const { return branches.begin() + i; }

    itemcount_t itemCount() const;

    node_branch &branch(keycount_t i) { return branches[i]; }

    void setBranch(size_t i, const node_ptr &node);

    branchlist_t branches;
};

typedef boost::shared_ptr<LeafNode> leafnode_ptr;
typedef boost::shared_ptr<OverflowNode> overflownode_ptr;
typedef boost::shared_ptr<InternalNode> internalnode_ptr;

struct node_branch {
    node_branch(const memslice &minKey, nodeid_t nodeID, itemcount_t itemCount)
        : minKey(minKey), nodeID(nodeID), itemCount(itemCount) { }
    node_branch(const memslice &minKey, const node_ptr &child)
        : minKey(minKey), nodeID(), itemCount(child->itemCount()), child(child) { }

    void inc() { itemCount++; }

    memslice minKey;
    nodeid_t nodeID;
    itemcount_t itemCount;

    node_ptr child; // Only valid while mutating the tree
};


/**
 * Return the index where the subtree for a particular key will be located
 *
 * POST: branch[ret-1] < branch[ret].key <= key
 */
keycount_t FindInternalKey(const internalnode_ptr &node, const memslice &key, const tree_functions &fns);

/**
 * Find the index where this item can go with the least amount of child nodes.
 *
 * POST: branch[ret].key <= key <= branch[ret+1].key
 */
keycount_t FindShallowestInternalKey(const internalnode_ptr &node, const memslice &key, const tree_functions &fns);

std::ostream &operator <<(std::ostream &os, const libbruce::Node &x);

}


#endif
