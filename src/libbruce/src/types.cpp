#include <libbruce/types.h>

namespace libbruce {

std::ostream &operator <<(std::ostream &os, const libbruce::nodeid_t &id)
{
    std::string ret(sizeof(libbruce::nodeid_t) * 2, 0);
    for (int i = 0; i < sizeof(libbruce::nodeid_t); i++)
        sprintf((char*)ret.data() + 2 * i, "%02X", id.data()[i]);
    os << ret;
    return os;
}

#define HDIGIT(c) ('0' <= c && c <= '9' ? c - '0' : \
                   'a' <= c && c <= 'f' ? c - 'a' + 10 : \
                   'A' <= c && c <= 'F' ? c - 'A' + 10 : 0)

std::istream &operator >>(std::istream &is, libbruce::nodeid_t &id)
{
    is >> std::hex;
    unsigned char *p = id.data();
    for (int i = 0; i < 20; i++)
    {
        unsigned char h, l, c;
        is >> h;
        is >> l;
        c = (HDIGIT(h) << 4) + HDIGIT(l);
        (*p++) = c;
    }
    return is;
}

}
