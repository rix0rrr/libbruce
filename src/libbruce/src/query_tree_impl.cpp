#include "query_tree_impl.h"

namespace bruce {

query_tree_impl::query_tree_impl(nodeid_t id, be::be &be, const tree_functions &fns)
    : m_id(id), m_be(be), m_fns(fns)
{
}

}
