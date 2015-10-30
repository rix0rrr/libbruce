#pragma once
#ifndef BRUCE_H
#define BRUCE_H

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/ref.hpp>
#include <stdexcept>

#include <libbruce/be/be.h>
#include <libbruce/be/mem.h>
#include <libbruce/be/disk.h>
#include <libbruce/tree.h>

namespace libbruce {

bool doFinish(be::be &blockEngine, mutation &mut, bool success);

template<typename K, typename V>
class bruce
{
public:
    typedef typename tree<K, V>::ptr tree_ptr;
    typedef typename tree<K, V>::iterator iterator;

    bruce(be::be &blockEngine)
        : m_blockEngine(blockEngine)
    {
    }

    /**
     * Create a new bruce tree
     *
     * The empty tree will not be backed by any block, because that would be
     * silly.
     */
    tree_ptr create()
    {
        return boost::make_shared<tree<K,V> >(maybe_nodeid(), boost::ref(m_blockEngine));
    }

    /**
     * Open an existing bruce tree
     */
    tree_ptr edit(const nodeid_t &id)
    {
        return boost::make_shared<tree<K, V> >(id, boost::ref(m_blockEngine));
    }

    /**
     * Query an existing bruce tree
     */
    tree_ptr query(const nodeid_t &id)
    {
        return boost::make_shared<tree<K,V> >(id, boost::ref(m_blockEngine));
    }

    /**
     * Commit or abort a given mutation.
     *
     * This should ALWAYS be called after flushing a tree.
     *
     * The success flag is combined with the success flag of the mutation. Only
     * if both are true, all obsolete blocks are deleted. Otherwise, the newly
     * created blocks are deleted.
     *
     * The mutation record is changed to remove deleted blocks from the
     * appropriate lists (i.e., after this function returns, it will contain
     * only IDs that STILL need to be deleted).
     *
     * Returns true iff all deletes succeeded.
     */
    bool finish(mutation &mut, bool success)
    {
        return doFinish(m_blockEngine, mut, success);
    }

    static tree_functions fns;
private:
    be::be &m_blockEngine;
};

template<typename K, typename V>
tree_functions bruce<K, V>::fns(&traits::convert<K>::compare, &traits::convert<V>::compare, &traits::convert<K>::size, &traits::convert<V>::size);

}

#endif
