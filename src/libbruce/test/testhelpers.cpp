#include "testhelpers.h"

#include <algorithm>

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
    return *a.at<uint32_t>(0) - *b.at<uint32_t>(0);
}

bruce::tree_functions intToIntTree(&intCompare, &intSize, &intSize);

uint32_t one = 1;
uint32_t two = 2;
uint32_t three = 3;
bruce::memory one_r(&one, sizeof(one));
bruce::memory two_r(&two, sizeof(two));
bruce::memory three_r(&three, sizeof(three));
