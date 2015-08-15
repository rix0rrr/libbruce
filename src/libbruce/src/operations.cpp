#include "operations.h"

namespace bruce {

insert_operation::insert_operation(be::be &be, maybe_blockid id, const range &key, const range &value, types::comparison_fn *compareKeys)
    : m_be(be), m_id(id), m_key(key), m_value(value), m_compareKeys(compareKeys)
{
}

void insert_operation::do_it()
{
    // Empty tree, just create a leaf node and return
    if (!m_id)
    {
        mut.setRoot(createNewLeafNode(m_be.newIdentifier()));
        return;
    }

    // Else search for the page where we're going to insert
    range b = m_be.get(*m_id);
}

be::blockid insert_operation::writeBuilderToPage(const be::blockid &newId)
{
    mut.addCreated(newId);
    return newId;
}

be::blockid insert_operation::createNewLeafNode(const be::blockid &newId)
{
    return newId;
}

/**
 * In the current internal node, find the next block to go down into
 *
 * Returns no blockid if the key we're looking for goes to the left of the leftmost key.
 */
/*
maybe_blockid insert_operation::findNextBlock(const bruce_fb::InternalNode *node)
{
    maybe_blockid ret;
    for (auto it = node->refs()->begin(); it != node->refs()->end(); ++it)
    {
        // FIXME: Binary search here, eh
        if (m_compareKeys(from_fb(it->firstKey()), m_key) > 1) break;
        ret = it->id();
    }
    return ret;
}
*/


}
