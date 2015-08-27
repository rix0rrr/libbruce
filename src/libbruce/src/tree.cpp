#include <libbruce/tree.h>

#include "operations.h"

namespace bruce {

unsafe_tree::unsafe_tree(const maybe_blockid &id, be::be &be, tree_functions fns)
    : m_id(id), m_be(be), m_fns(fns)
{
}

mutation unsafe_tree::insert(const memory &key, const memory &value)
{
    /*
    insert_operation insert(m_be, m_id, key, value, m_compareKeys);

    try
    {
        insert.do_it();
    }
    catch (std::runtime_error &e)
    {
        insert.mut.fail(e.what());
    }
    catch (...)
    {
        insert.mut.fail("Unexpected exception type");
    }

    return insert.mut;
    */

    return mutation();
}

}
