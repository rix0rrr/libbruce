#ifndef BRUCE_QUERY_TREE_H
#define BRUCE_QUERY_TREE_H

#include <libbruce/types.h>
#include <libbruce/be/be.h>

#include "tree_impl.h"

namespace bruce {

struct query_tree_impl : private tree_impl
{
    query_tree_impl(be::be &be, nodeid_t rootID, const tree_functions &fns);
private:
};

}

#endif
