#ifndef BRUCE_TYPES_H
#define BRUCE_TYPES_H

// Conversions for converting types to byte ranges

#include <libbruce/memory.h>
#include <arpa/inet.h>
#include <algorithm>

namespace bruce { namespace types {

//----------------------------------------------------------------------

/**
 * Default implementation of type traits
 *
 * We assume this will be used for number types, which will be little-endian
 * (so we compare them differently).
 */
template<typename T>
struct convert
{
    static range to_bytes(const T &t)
    {
        return range(&t, sizeof(t));
    }

    static T from_bytes(const range &r)
    {
        return *((T*)r.ptr());
    }

    static int compare(const range &a, const range &b)
    {
        // Assume types are little-endian, so compare from the back
        if (a.size() != b.size()) throw std::runtime_error("Type sizes not equal");

        for (const uint8_t *ap = a.byte_ptr() + a.size(), *bp = b.byte_ptr() + b.size(); a.byte_ptr() <= ap; ap--, bp--)
        {
            if (*ap < *bp) return -1;
            if (*ap > *bp) return 1;
        }
        return 0;
    }
};

//----------------------------------------------------------------------

template<>
struct convert<std::string>
{
    static range to_bytes(const std::string &t)
    {
        return range(t.c_str(), t.size());
    }

    static std::string from_bytes(const range &r)
    {
        return std::string((char*)r.ptr(), r.size());
    }

    static int compare(const range &a, const range &b)
    {
        int r = bcmp(a.ptr(), b.ptr(), std::min(a.size(), b.size()));
        if (r != 0) return r;
        return a.size() < b.size() ? -1 : 1;
    }
};

typedef int comparison_fn(const range &, const range &);

}}

#endif
