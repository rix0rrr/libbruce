#ifndef BRUCE_H
#define BRUCE_H

#include <stdexcept>

#include <libbruce/tree.h>
#include <libbruce/be/be.h>
#include <libbruce/be/mem.h>

namespace bruce {

class bruce {
public:
    bruce(be::be &blockEngine);

    /**
     * Create a new bruce tree
     *
     * The empty tree will not be backed by any block, because that would be
     * silly.
     */
    template<typename K, typename V>
    tree<K, V> create()
    {
        return tree<K, V>(maybe_blockid(), m_blockEngine);
    }

    /**
     * Open an existing bruce tree
     */
    template<typename K, typename V>
    tree<K, V> open(const be::blockid &id);

    void cleanup(/*nodelist*/);
private:
    be::be &m_blockEngine;
};

}

#endif
