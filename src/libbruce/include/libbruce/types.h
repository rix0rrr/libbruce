#ifndef BRUCE_TYPES_H
#define BRUCE_TYPES_H

#include <utility>

#include <libbruce/memory.h>

// Standard types

namespace bruce {

typedef uint16_t keycount_t;
typedef uint64_t nodeid_t;
typedef uint32_t itemcount_t;

namespace fn {

typedef uint32_t sizeinator(const void *);
typedef int comparinator(const memory &, const memory &);

}

struct tree_functions
{
    tree_functions(fn::comparinator *keyCompare, fn::sizeinator *keySize, fn::sizeinator *valueSize)
        : keyCompare(keyCompare), keySize(keySize), valueSize(valueSize) { }

    fn::comparinator *keyCompare;
    fn::sizeinator *keySize;
    fn::sizeinator *valueSize;
};


}


#endif
