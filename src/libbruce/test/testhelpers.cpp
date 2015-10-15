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
    for (libbruce::be::mem::blockmap_t::iterator it = mem.blocks().begin(); it != mem.blocks().end(); ++it)
    {
        std::cout << "Block[" << it->first << "] => " << *ParseNode(it->second, fns) << std::endl;
    }
}

nodeid_t putNode(be::mem &mem, const node_ptr &node)
{
    memory block = SerializeNode(node);
    nodeid_t id = mem.id(block);
    be::putblocklist_t blocks;
    blocks.push_back(be::putblock_t(id, block));
    mem.put_all(blocks);
    return id;
}

//----------------------------------------------------------------------

make_leaf::make_leaf(const tree_functions &fns)
    : leaf(boost::make_shared<LeafNode>(fns))
{
}

make_leaf &make_leaf::kv(const memory &k, const memory &v)
{
    leaf->insert(kv_pair(k, v));
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
    return put_result(leaf, putNode(mem, leaf));
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
    return put_result(internal, putNode(mem, internal));
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
    return put_result(overflow, putNode(mem, overflow));
}

}

std::ostream &operator <<(std::ostream &os, libbruce::be::mem &x)
{
    for (libbruce::be::mem::blockmap_t::iterator it = x.blocks().begin(); it != x.blocks().end(); ++it)
    {
        std::cout << "Block[" << it->first << "] => " << it->second << std::endl;
    }
    return os;
}

