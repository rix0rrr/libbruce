#pragma once
#ifndef BRUCE_NODES_H
#define BRUCE_NODES_H

#include <utility>
#include <libbruce/memslice.h>
#include <libbruce/types.h>
#include <boost/make_shared.hpp>

#include <vector>
#include <map>

namespace libbruce {

//----------------------------------------------------------------------
//  General types
//

class Node;
class LeafNode;
class OverflowNode;
class InternalNode;

typedef boost::shared_ptr<Node> node_ptr;
typedef boost::shared_ptr<LeafNode> leafnode_ptr;
typedef boost::shared_ptr<OverflowNode> overflownode_ptr;
typedef boost::shared_ptr<InternalNode> internalnode_ptr;

enum node_type_t {
    TYPE_LEAF,
    TYPE_INTERNAL,
    TYPE_OVERFLOW
};

enum edit_t {
    INSERT, REMOVE_KEY, REMOVE_KV, UPSERT
};

struct pending_edit
{
    pending_edit(edit_t edit, const memslice &key, const memslice &value, bool guaranteed)
        : edit(edit), key(key), value(value), guaranteed(guaranteed) { }

    edit_t edit;
    memslice key;
    memslice value;
    bool guaranteed;

    int delta() const;
};

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

    virtual void print(std::ostream &os) const = 0;
private:
    node_type_t m_nodeType;
};

std::ostream &operator <<(std::ostream &os, const libbruce::Node &x);

extern memslice g_emptyMemory;

}


#endif
