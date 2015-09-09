#ifndef BRUCE_QUERY_TREE_H
#define BRUCE_QUERY_TREE_H

#include <libbruce/types.h>
#include <libbruce/be/be.h>

namespace bruce {

struct query_tree_impl
{
    query_tree_impl(nodeid_t id, be::be &be, const tree_functions &fns);

private:
    nodeid_t m_id;
    be::be &m_be;
    tree_functions m_fns;
};

}

#endif
