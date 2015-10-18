#pragma once
#ifndef BRUCE_TRAITS_H
#define BRUCE_TRAITS_H

// Conversions for converting types to byte ranges

#include <libbruce/mempool.h>
#include <libbruce/types.h>
#include <arpa/inet.h>
#include <algorithm>

#include <boost/lexical_cast.hpp>

namespace libbruce {

namespace traits {

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
    static memslice to_bytes(const T &t, mempool &pool)
    {
        // Copying here is not awesome, but not really a good way around it :(
        memslice m = pool.alloc(sizeof(t));
        memcpy((void*)m.ptr(), &t, sizeof(t));
        return m;
    }

    static T from_bytes(const memslice &r)
    {
        return *((T*)r.ptr());
    }

    static int compare(const memslice &a, const memslice &b)
    {
        // Assume types are little-endian, so compare from the back
        if (a.size() != b.size())
            throw std::runtime_error((std::string("Type sizes not equal. ") + boost::lexical_cast<std::string>(a.size()) + " != " + boost::lexical_cast<std::string>(b.size())).c_str());

        const T &aa = *a.at<T>(0);
        const T &bb = *b.at<T>(0);

        if (aa < bb) return -1;
        if (aa > bb) return 1;
        return 0;
    }

    static uint32_t size(const void*)
    {
        return sizeof(T);
    }
};

//----------------------------------------------------------------------

template<>
struct convert<std::string>
{
    static memslice to_bytes(const std::string &t, mempool &pool)
    {
        memslice m = pool.alloc(t.size() + 1);
        memcpy((void*)m.ptr(), t.c_str(), t.size() + 1);
        return m;
    }

    static std::string from_bytes(const memslice &r)
    {
        return std::string((char*)r.ptr());
    }

    static int compare(const memslice &a, const memslice &b)
    {
        return strcmp((char*)a.ptr(), (char*)b.ptr());
    }

    static uint32_t size(const void* x)
    {
        return strlen((char*)x) + 1;
    }
};

//----------------------------------------------------------------------

template<>
struct convert<binary>
{
    static memslice to_bytes(const binary &t, mempool &pool)
    {
        memslice m = pool.alloc(t.size() + sizeof(uint32_t));
        *m.at<uint32_t>(0) = t.size();
        memcpy((void*)(m.ptr() + sizeof(uint32_t)), t.c_str(), t.size());
        return m;
    }

    static binary from_bytes(const memslice &r)
    {
        return binary((char*)(r.ptr() + sizeof(uint32_t)), *r.at<uint32_t>(0));
    }

    static int compare(const memslice &a, const memslice &b)
    {
        int r = bcmp(a.ptr() + sizeof(uint32_t), b.ptr() + sizeof(uint32_t), std::min(a.size(), b.size()) - sizeof(uint32_t));
        if (r != 0) return r;
        return a.size() < b.size() ? -1 : 1;
    }

    static uint32_t size(const void* x)
    {
        return *(uint32_t*)x + sizeof(uint32_t);
    }
};

}}

#endif
