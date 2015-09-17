#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <libbruce/bruce.h>
#include "nodes.h"

namespace bruce {

/**
 * Return the size of a 32-bit int
 */
uint32_t intSize(const void *);
int intCompare(const memory &, const memory &);

int rngcmp(const memory &a, const memory &b);

memory intCopy(uint32_t i);

extern tree_functions intToIntTree;

extern memory one_r;
extern memory two_r;
extern memory three_r;

void printMem(be::mem &mem, const tree_functions &fns);
void putNode(be::mem &mem, nodeid_t id, const node_ptr &node);

//----------------------------------------------------------------------
// BUILDERS
//
// Below are builder classes which are useful in setting up circumstances for tests to test.

struct put_result
{
    put_result(const node_ptr &node, nodeid_t nodeID) : node(node), nodeID(nodeID) { }

    node_ptr node;
    nodeid_t nodeID;
};

struct make_leaf
{
    make_leaf();
    make_leaf &kv(const memory &k, const memory &v);
    make_leaf &kv(uint32_t k, uint32_t v);
    make_leaf &overflow(const put_result &put);
    put_result put(be::mem &mem);

private:
    leafnode_ptr leaf;
};

struct make_internal
{
    make_internal();
    make_internal &brn(const put_result &put);
    put_result put(be::mem &mem);

private:
    internalnode_ptr internal;
};

struct make_overflow
{
    make_overflow();
    make_overflow &val(const memory &value);
    make_overflow &val(uint32_t v);
    make_overflow &next(const put_result &put);
    put_result put(be::mem &mem);

private:
    overflownode_ptr overflow;
};

}

std::ostream &operator <<(std::ostream &os, bruce::be::mem &x);

#endif
