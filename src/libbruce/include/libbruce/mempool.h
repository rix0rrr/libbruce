#pragma once
#ifndef LIBBRUCE_MEMPOOL_H
#define LIBBRUCE_MEMPOOL_H

#include <list>
#include <boost/noncopyable.hpp>

#include <libbruce/mempage.h>

namespace libbruce {

/**
 * A collection to keep a set of mempages alive
 *
 * Retains a special set of mempages to allocate new memslices out of, used for
 * serializing new consumer-passed key/values into.
 */
struct mempool : private boost::noncopyable
{
    mempool();

    /**
     * Retain a reference to particular page
     */
    void retain(const mempage &page);

    /**
     * Allocate a memslice of the given size
     */
    memory alloc(size_t size);
private:
    std::list<mempage> m_pages;
    mempage m_allocPage;
    size_t m_allocOffset;
};

}

#endif
