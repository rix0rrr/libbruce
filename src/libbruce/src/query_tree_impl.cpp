#include "query_tree_impl.h"

namespace bruce {

query_tree_impl::query_tree_impl(be::be &be, nodeid_t rootID, const tree_functions &fns)
    : tree_impl(be, rootID, fns)
{
}

}
