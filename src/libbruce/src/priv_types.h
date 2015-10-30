#pragma once
#ifndef PRIVATE_TYPES_H
#define PRIVATE_TYPES_H

#include <libbruce/types.h>

namespace libbruce {

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

struct EditOrder : public KeyOrder
{
    EditOrder(const tree_functions &fns) : KeyOrder(fns) { }

    bool operator()(const memslice &a, const pending_edit &b) const
    {
        return KeyOrder::operator()(a, b.key);
    }

    bool operator()(const pending_edit &a, const memslice &b) const
    {
        return KeyOrder::operator()(a.key, b);
    }
};

typedef std::pair<memslice, memslice> kv_pair;

struct PairOrder : public KeyOrder
{
    PairOrder(const tree_functions &fns) : KeyOrder(fns) { }

    bool operator()(const memslice &a, const kv_pair &b) const { return KeyOrder::operator()(a, b.first); }
    bool operator()(const kv_pair &a, const memslice &b) const { return KeyOrder::operator()(a.first, b); }
};


struct node_branch {
    node_branch(const memslice &minKey, nodeid_t nodeID, itemcount_t itemCount);
    node_branch(const memslice &minKey, const node_ptr &child);

    void inc() { itemCount++; }

    memslice minKey;
    nodeid_t nodeID;
    itemcount_t itemCount;

    node_ptr child; // Only valid while mutating the tree
};
typedef std::vector<node_branch> branchlist_t;

enum Depth {
    SHALLOW, DEEP
};

struct splitresult_t {
    splitresult_t(node_ptr left)
    {
        assert(left);
        branches.push_back(node_branch(memslice(), left));
    }

    splitresult_t(node_ptr left, const memslice& splitKey, node_ptr right)
    {
        assert(left);
        assert(right);
        branches.push_back(node_branch(memslice(), left));
        branches.push_back(node_branch(splitKey, right));
    }

    node_branch &left() { return *branches.begin(); }
    const node_branch &left() const{ return *branches.begin(); }

    bool didSplit() const { return branches.size() > 1; }
    branchlist_t branches;
};


}

#endif
