#include "testhelpers.h"

#include <algorithm>
#include <boost/make_shared.hpp>
#include "nodes.h"
#include "serializing.h"
#include <ostream>

namespace libbruce {

uint32_t intSize(const void *)
{
    return sizeof(uint32_t);
}

int rngcmp(const memory &a, const memory &b)
{
    int ret = memcmp(a.ptr(), b.ptr(), std::min(a.size(), b.size()));
    if (ret != 0) return ret;
    return a.size() - b.size();
}

int intCompare(const memory &a, const memory &b)
{
    uint32_t aa = *a.at<uint32_t>(0);
    uint32_t bb = *b.at<uint32_t>(0);

    if (aa < bb) return -1;
    if (aa > bb) return 1;
    return 0;
}

memory intCopy(uint32_t i)
{
    boost::shared_array<char> x(new char[sizeof(i)]);
    *(uint32_t*)x.get() = i;
    return memory(x, sizeof(i));
}

tree_functions intToIntTree(&intCompare, &intCompare, &intSize, &intSize);

uint32_t one = 1;
uint32_t two = 2;
uint32_t three = 3;
memory one_r(&one, sizeof(one));
memory two_r(&two, sizeof(two));
memory three_r(&three, sizeof(three));

void printMem(be::mem &mem, const tree_functions &fns)
{
    for (nodeid_t i = 0; i < mem.blockCount(); i++)
    {
        memory m(mem.get(i));
        std::cout << "Block[" << i << "] => " << *ParseNode(m, fns) << std::endl;
    }
}

void putNode(be::mem &mem, nodeid_t id, const node_ptr &node)
{
    be::putblocklist_t blocks;
    blocks.push_back(be::putblock_t(id, SerializeNode(node)));
    mem.put_all(blocks);
}

//----------------------------------------------------------------------

make_leaf::make_leaf()
    : leaf(boost::make_shared<LeafNode>())
{
}

make_leaf &make_leaf::kv(const memory &k, const memory &v)
{
    leaf->append(kv_pair(k, v));
    return *this;
}

make_leaf &make_leaf::kv(uint32_t k, uint32_t v)
{
    return kv(intCopy(k), intCopy(v));
}

make_leaf &make_leaf::overflow(const put_result &put)
{
    leaf->overflow.count = put.node->itemCount();
    leaf->overflow.nodeID = put.nodeID;
    return *this;
}

put_result make_leaf::put(be::mem &mem)
{
    std::vector<nodeid_t> ids;
    mem.newIdentifiers(1, &ids);
    putNode(mem, ids[0], leaf);
    return put_result(leaf, ids[0]);
}

//----------------------------------------------------------------------

make_internal::make_internal()
    : internal(boost::make_shared<InternalNode>())
{
}

make_internal &make_internal::brn(const put_result &put)
{
    internal->append(node_branch(put.node->minKey(), put.nodeID, put.node->itemCount()));
    return *this;
}

put_result make_internal::put(be::mem &mem)
{
    std::vector<nodeid_t> ids;
    mem.newIdentifiers(1, &ids);
    putNode(mem, ids[0], internal);
    return put_result(internal, ids[0]);
}

//----------------------------------------------------------------------

make_overflow::make_overflow()
    : overflow(boost::make_shared<OverflowNode>())
{
}

make_overflow &make_overflow::val(const memory &value)
{
    overflow->append(value);
    return *this;
}

make_overflow &make_overflow::val(uint32_t i)
{
    return val(intCopy(i));
}

make_overflow &make_overflow::next(const put_result &put)
{
    overflow->next.nodeID = put.nodeID;
    overflow->next.count = put.node->itemCount();
    return *this;
}

put_result make_overflow::put(be::mem &mem)
{
    std::vector<nodeid_t> ids;
    mem.newIdentifiers(1, &ids);
    putNode(mem, ids[0], overflow);
    return put_result(overflow, ids[0]);
}

}

std::ostream &operator <<(std::ostream &os, libbruce::be::mem &x)
{
    for (libbruce::nodeid_t i = 0; i < x.blockCount(); i++)
    {
        std::cout << "Block[" << i << "] => " << x.get(i) << std::endl;
    }
    return os;
}

