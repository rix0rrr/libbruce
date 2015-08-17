#include <libbruce/tree.h>

#include "operations.h"

namespace bruce {

unsafe_tree::unsafe_tree(const maybe_blockid &id, be::be &be, types::comparison_fn compareKeys)
    : m_id(id), m_be(be), m_compareKeys(compareKeys)
{
}

mutation unsafe_tree::insert(const memory &key, const memory &value)
{
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
}

}
