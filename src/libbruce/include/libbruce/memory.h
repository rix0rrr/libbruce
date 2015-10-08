#pragma once
#ifndef BRUCE_MEMORY_H
#define BRUCE_MEMORY_H

#include <boost/shared_array.hpp>
#include <iostream>
#include <stdint.h>
#include <stdexcept>

namespace libbruce {

/**
 * A slice of borrowed or owned memory
 *
 * This class fulfills a number of roles:
 *
 * - In principle, it just points at a slice of memory. This usage replaces a
 *   pair of iterators, which make for a poor API. This is used when passing
 *   a memory block INTO a function.
 *
 * - It may optionally share ownership (via a shared_array) of a block of memory.
 *   This is used when returning a memory block FROM a function. Note that the
 *   calling function is unaware of whether the memory is owned or not: this
 *   is completely up to the producer of the slice.
 *
 * We could have used std::vector<> for this, but that would always require
 * copying, even when it's not necessary.
 */
struct memory
{
    typedef boost::shared_array<char> memptr;

    memory()
        : m_ptr(NULL), m_size(0)
    {
    }

    memory(const void *ptr, size_t size)
        : m_ptr(ptr), m_size(size)
    {
    }

    memory(const void *ptr, size_t size, memptr mem)
        : m_ptr(ptr), m_size(size), m_mem(mem)
    {
    }

    memory(memptr mem, size_t size)
        : m_ptr(mem.get()), m_size(size), m_mem(mem)
    {
    }

    memory(size_t size)
        : m_mem(new char[size]), m_size(size)
    {
        m_ptr = m_mem.get();
    }

    bool empty() const { return m_size == 0; }
    const void *ptr() const { return m_ptr; }
    const uint8_t *byte_ptr() const { return (uint8_t*)m_ptr; }
    size_t size() const { return m_size; }

    /**
     * Return a slice into this memory
     *
     * Ownership will be shared if possible.
     */
    memory slice(size_t offset, size_t size)
    {
        return memory(byte_ptr() + offset, size, m_mem);
    }

    /**
     * Get a typed read/write pointer to the data, only iff owned memory
     *
     * Mostly a convenience function for buffer writers.
     */
    template<typename T>
    T* at(size_t offset)
    {
        if (!m_mem) throw std::runtime_error("Can't change non-owned memory");
        // Pointers don't have to be the same, but we need to be backed by owned memory
        return (T*)((char*)m_ptr + offset);
    }

    /**
     * Get a typed read pointer to the data
     *
     * Mostly a convenience function for buffer parsers.
     */
    template<typename T>
    const T* at(size_t offset) const
    {
        return (T*)((const char*)m_ptr + offset);
    }

    bool operator==(const memory &other) const;
private:
    const void *m_ptr;
    size_t m_size;
    memptr m_mem;
};

std::ostream &operator <<(std::ostream &os, const libbruce::memory &m);

}

#endif
