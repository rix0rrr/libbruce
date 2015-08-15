#include <libbruce/memory.h>

const char* g_hex = "0123456789abcdef";

std::ostream &operator <<(std::ostream &os, const bruce::range &m)
{
    for (int i = 0; i < m.size(); i++)
    {
        char c = *m.at<char>(i);
        if (i) os << " ";
        os << g_hex[c >> 4 & 0xF] << g_hex[c & 0xF];
    }

    return os;
}
