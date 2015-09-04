#ifndef BRUCE_H
#define BRUCE_H

#include <stdexcept>

#include <libbruce/tree.h>
#include <libbruce/be/be.h>
#include <libbruce/be/mem.h>

namespace bruce {

class bruce
{
public:
    bruce(be::be &blockEngine);

    /**
     * Create a new bruce tree
     *
     * The empty tree will not be backed by any block, because that would be
     * silly.
     */
    template<typename K, typename V>
    typename tree<K, V>::ptr create()
    {
        return typename boost::shared_ptr<tree<K,V> >(new tree<K, V>(maybe_nodeid(), m_blockEngine));
    }

    /**
     * Open an existing bruce tree
     */
    template<typename K, typename V>
    typename tree<K, V>::ptr open(const nodeid_t &id)
    {
        return typename boost::shared_ptr<tree<K,V> >(new tree<K, V>(id, m_blockEngine));
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
    bool finish(mutation &mut, bool success);
private:
    be::be &m_blockEngine;
};

}

#endif
