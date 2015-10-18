#pragma once
#ifndef BRUCE_MEMSLICE_H
#define BRUCE_MEMSLICE_H

#include <boost/shared_array.hpp>
#include <iostream>
#include <stdint.h>
#include <stdexcept>

namespace libbruce {

/**
 * A slice of a mempage
 *
 * This class points at a slice of memory. This usage replaces a pair of
 * iterators, which make for a poor API. This is used when passing a memory
 * block INTO a function.
 *
 * It's movable since it needs to be relocatable a lot inside nodes.
 */
struct memslice
{
    memslice()
        : m_ptr(NULL), m_size(0)
    {
    }

    memslice(void *ptr, size_t size)
        : m_ptr((uint8_t*)ptr), m_size(size)
    {
    }

    bool empty() const { return m_size == 0; }
    const uint8_t *ptr() const { return m_ptr; }
    uint8_t *ptr() { return m_ptr; }
    size_t size() const { return m_size; }

    /**
     * Get a typed read/write pointer to the data, only iff owned memory
     *
     * Mostly a convenience function for buffer writers.
     */
    template<typename T>
    T* at(size_t offset)
    {
        return (T*)(m_ptr + offset);
    }

    /**
     * Get a typed read pointer to the data
     *
     * Mostly a convenience function for buffer parsers.
     */
    template<typename T>
    const T* at(size_t offset) const
    {
        return (const T*)(m_ptr + offset);
    }

    bool operator==(const memslice &other) const;
private:
    uint8_t *m_ptr;
    size_t m_size;
};

std::ostream &operator <<(std::ostream &os, const libbruce::memslice &m);

}

#endif
