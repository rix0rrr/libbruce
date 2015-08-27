#ifndef BRUCE_NODES_H
#define BRUCE_NODES_H

#include <utility>
#include <libbruce/memory.h>
#include <libbruce/types.h>
#include <boost/make_shared.hpp>

#include <vector>

namespace bruce {

class Node;

typedef boost::shared_ptr<Node> node_ptr;

struct kv_pair
{
    kv_pair(const memory &key, const memory &value)
        : key(key), value(value) { }

    memory key;
    memory value;
};

typedef std::vector<kv_pair> pairlist_t;

struct node_branch {
    node_branch(const memory &minKey, nodeid_t nodeID, itemcount_t itemCount)
        : minKey(minKey), nodeID(nodeID), itemCount(itemCount) { }
    node_branch(const memory &minKey, const node_ptr &child, itemcount_t itemCount)
        : minKey(minKey), nodeID(0), itemCount(itemCount), child(child) { }

    void inc() { itemCount++; }

    memory minKey;
    nodeid_t nodeID;
    itemcount_t itemCount;
    node_ptr child; // Only valid while mutating the tree
};

typedef std::vector<node_branch> branchlist_t;


/**
 * Base class for Nodes
 */
struct Node
{
    virtual ~Node();

    virtual bool isLeafNode() const = 0;
    virtual keycount_t count() const = 0; // Items in this node
    virtual keycount_t itemCount() const = 0; // Items in this node and below
    virtual const memory &minKey() const = 0;
};

/**
 * Leaf node type
 */
struct LeafNode : public Node
{
    LeafNode(size_t sizeHint=0);
    LeafNode(pairlist_t::iterator begin, pairlist_t::iterator end);

    bool isLeafNode() const { return true; }
    keycount_t count() const { return m_pairs.size(); }
    virtual const memory &minKey() const;
    virtual keycount_t itemCount() const;

    void insert(size_t i, const kv_pair &item) { m_pairs.insert(m_pairs.begin() + i, item); }

    kv_pair &pair(keycount_t i) { return m_pairs[i]; }
    pairlist_t &pairs() { return m_pairs; }
private:
    pairlist_t m_pairs;
};

/**
 * Internal node type
 */
struct InternalNode : public Node
{
    InternalNode(size_t sizeHint=0);
    InternalNode(branchlist_t::iterator begin, branchlist_t::iterator end);

    bool isLeafNode() const { return false; }
    virtual keycount_t count() const { return m_branches.size(); }
    virtual const memory &minKey() const;

    void insert(size_t i, const node_branch &branch) { m_branches.insert(m_branches.begin() + i, branch); }

    keycount_t itemCount() const;

    node_branch &branch(keycount_t i) { return m_branches[i]; }
    branchlist_t &branches() { return m_branches; }
private:
    branchlist_t m_branches;
};

typedef boost::shared_ptr<LeafNode> leafnode_ptr;
typedef boost::shared_ptr<InternalNode> internalnode_ptr;

/**
 * Return the index where to INSERT a particular key.
 *
 * POST: leaf[ret-1].key <= key < leaf[ret].key
 */
keycount_t FindLeafKey(const leafnode_ptr &leaf, const memory &key, const tree_functions &fns);

/**
 * Return the index where the subtree for a particular key is located
 *
 * POST: branch[ret].key <= key < branch[ret+1].key
 *       \/ (ret == 0  && key < branch[1].key)
 */
keycount_t FindInternalKey(const internalnode_ptr &leaf, const memory &key, const tree_functions &fns);

}

#endif
