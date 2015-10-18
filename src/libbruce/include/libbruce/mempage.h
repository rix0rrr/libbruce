#pragma once
#ifndef BRUCE_MEMPAGE_H
#define BRUCE_MEMPAGE_H

#include <boost/shared_array.hpp>
#include <boost/smart_ptr/make_shared_array.hpp>
#include <iostream>
#include <stdint.h>
#include <stdexcept>

#include <libbruce/memslice.h>

namespace libbruce {

/**
 * A page of memory, whose ownership can be shared
 *
 * mempages are used for two purposes:
 *
 * - To storage pages retrieved from a block engine.
 * - To keep serialized keys and values from the consumer.
 *
 * mempages keep track of ownership and, through a block pool, are guaranteed to
 * live as long as the bruce trees are in memory. This removes the need for
 * tracking ownership in the memslices, improving their performance.
 */
struct mempage
{
    typedef boost::shared_array<uint8_t> memptr;

    mempage()
        : m_size(0)
    {
    }

    mempage(size_t size)
        : m_mem(new uint8_t[size]()), m_size(size)
    {
    }

    bool empty() const { return m_size == 0; }
    uint8_t *ptr() { return m_mem.get(); }
    const uint8_t *ptr() const { return m_mem.get(); }
    size_t size() const { return m_size; }

    /**
     * Return a slice into this mempage
     *
     * Ownership will be shared if possible.
     */
    memslice slice(size_t offset, size_t size)
    {
        return memslice(ptr() + offset, size);
    }

    memslice all()
    {
        return memslice(ptr(), m_size);
    }

    /**
     * Get a typed read/write pointer to the data
     */
    template<typename T>
    T* at(size_t offset)
    {
        return (T*)(m_mem.get() + offset);
    }

    /**
     * Get a typed read pointer to the data
     */
    template<typename T>
    const T* at(size_t offset) const
    {
        return (const T*)(m_mem.get() + offset);
    }

private:
    size_t m_size;
    memptr m_mem;
};

}

#endif
