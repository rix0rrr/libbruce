#ifndef BRUCE_OPERATIONS_H
#define BRUCE_OPERATIONS_H

#include <libbruce/mutation.h>
#include <libbruce/be/be.h>
#include <libbruce/types.h>
#include <libbruce/tree.h>

namespace bruce {

struct insert_operation
{
    insert_operation(be::be &be, maybe_blockid id, const range &key, const range &value, types::comparison_fn *compareKeys);

    mutation mut;

    void do_it();
private:
    maybe_blockid m_id;
    be::be &m_be;
    const range &m_key;
    const range &m_value;
    types::comparison_fn *m_compareKeys;

    be::blockid createNewLeafNode(const be::blockid &newId);
    be::blockid writeBuilderToPage(const be::blockid &newId);
    //maybe_blockid findNextBlock(const bruce_fb::InternalNode *);
};

}

#endif
