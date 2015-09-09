#include "tree_impl.h"

namespace bruce {


tree_impl::tree_impl(be::be &be, maybe_nodeid rootID, const tree_functions &fns)
    : m_be(be), m_rootID(rootID), m_fns(fns)
{
}

node_ptr &tree_impl::root()
{
    if (!m_root)
    {
        if (m_rootID)
            m_root = load(*m_rootID);
        else
            // No existing tree, create a new leaf node
            m_root = boost::make_shared<LeafNode>();
    }

    return m_root;
}

const node_ptr &tree_impl::child(node_branch &branch)
{
    if (!branch.child) branch.child = load(branch.nodeID);
    return branch.child;
}

node_ptr tree_impl::load(nodeid_t id)
{
    m_oldIDs.push_back(id);
    memory mem = m_be.get(id);
    return ParseNode(mem, m_fns);
}

}
