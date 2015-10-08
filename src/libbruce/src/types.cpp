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

}
