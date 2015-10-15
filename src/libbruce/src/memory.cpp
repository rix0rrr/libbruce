#include <libbruce/memory.h>

#include <stdlib.h>
#include <string.h>

const char* g_hex = "0123456789abcdef";

namespace libbruce
{

bool memory::operator==(const memory &other) const
{
    if (size() != other.size()) return false;
    if (ptr() == other.ptr()) return true; // Easy win

    return memcmp(ptr(), other.ptr(), size()) == 0;
}

std::ostream &operator <<(std::ostream &os, const libbruce::memory &m)
{
    for (int i = 0; i < m.size(); i++)
    {
        char c = *m.at<char>(i);
        if (i) os << " ";
        os << g_hex[c >> 4 & 0xF] << g_hex[c & 0xF];
    }

    return os;
}

}
