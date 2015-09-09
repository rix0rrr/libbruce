#ifndef BRUCE_TREE_IMPL_H
#define BRUCE_TREE_IMPL_H

#include <libbruce/be/be.h>
#include <libbruce/types.h>
#include "nodes.h"

namespace bruce {

struct tree_impl
{
    tree_impl(be::be &be, maybe_nodeid rootID, const tree_functions &fns);

protected:
    be::be &m_be;
    maybe_nodeid m_rootID;
    tree_functions m_fns;

    node_ptr m_root;

    node_ptr &root();
    const node_ptr &child(node_branch &branch);
    node_ptr load(nodeid_t id);
};

}

#endif
