#pragma once
#ifndef BRUCE_TYPES_H
#define BRUCE_TYPES_H

#include <utility>
#include <boost/optional.hpp>

#include <libbruce/memory.h>

// Standard types

namespace libbruce {

typedef uint16_t keycount_t;
typedef uint32_t itemcount_t;

struct nodeid_t
{
    nodeid_t()
    {
        memset(m_bytes, 0, sizeof(m_bytes));
    }

    nodeid_t(const char *src)
    {
        memcpy(m_bytes, src, sizeof(m_bytes));
    }

    nodeid_t(size_t x)
    {
        memset(m_bytes, 0, sizeof(m_bytes));
        *(size_t*)m_bytes = x;
    }

    const char *data() const { return m_bytes; }
    char *data() { return m_bytes; }

    bool empty() const
    {
        for (int i = 0; i < sizeof(m_bytes); i++)
            if (m_bytes[i] != 0)
                return false;
        return true;
    }

    bool operator==(const nodeid_t &other) const { return memcmp(m_bytes, other.m_bytes, sizeof(m_bytes)) == 0; }
    bool operator<(const nodeid_t &other) const { return memcmp(m_bytes, other.m_bytes, sizeof(m_bytes)) < 0; }
private:
    char m_bytes[20];
};

std::ostream &operator <<(std::ostream &os, const libbruce::nodeid_t &id);

typedef boost::optional<nodeid_t> maybe_nodeid;

namespace fn {

typedef uint32_t sizeinator(const void *);
typedef int comparinator(const memory &, const memory &);

}

struct tree_functions
{
    tree_functions(fn::comparinator *keyCompare, fn::comparinator *valueCompare, fn::sizeinator *keySize, fn::sizeinator *valueSize)
        : keyCompare(keyCompare), valueCompare(valueCompare), keySize(keySize), valueSize(valueSize) { }

    fn::comparinator *keyCompare;
    fn::comparinator *valueCompare;
    fn::sizeinator *keySize;
    fn::sizeinator *valueSize;
};

/**
 * A type tagged alias for std::string which is serialized in a binary-safe way
 */
class binary : public std::string {
public:
    binary(const char *c, size_t n) : std::string(c, n) { }
};

}


#endif
