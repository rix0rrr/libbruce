#include "testhelpers.h"

#include <algorithm>
#include <boost/make_shared.hpp>
#include "nodes.h"
#include "serializing.h"

uint32_t intSize(const void *)
{
    return sizeof(uint32_t);
}

int rngcmp(const bruce::memory &a, const bruce::memory &b)
{
    int ret = memcmp(a.ptr(), b.ptr(), std::min(a.size(), b.size()));
    if (ret != 0) return ret;
    return a.size() - b.size();
}

int intCompare(const bruce::memory &a, const bruce::memory &b)
{
    uint32_t aa = *a.at<uint32_t>(0);
    uint32_t bb = *b.at<uint32_t>(0);

    if (aa < bb) return -1;
    if (aa > bb) return 1;
    return 0;
}

bruce::memory intCopy(uint32_t i)
{
    boost::shared_ptr<char> x(new char[sizeof(i)]);
    *(uint32_t*)x.get() = i;
    return bruce::memory(x, sizeof(i));
}

bruce::tree_functions intToIntTree(&intCompare, &intCompare, &intSize, &intSize);

uint32_t one = 1;
uint32_t two = 2;
uint32_t three = 3;
bruce::memory one_r(&one, sizeof(one));
bruce::memory two_r(&two, sizeof(two));
bruce::memory three_r(&three, sizeof(three));

std::ostream &operator <<(std::ostream &os, bruce::be::mem &x)
{
    for (bruce::nodeid_t i = 0; i < x.blockCount(); i++)
    {
        std::cout << "Block[" << i << "] => " << x.get(i) << std::endl;
    }
    return os;
}

