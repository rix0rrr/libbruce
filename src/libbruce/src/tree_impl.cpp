#include "tree_impl.h"

#include "serializing.h"
#include "leaf_node.h"
#include "internal_node.h"
#include "overflow_node.h"

namespace libbruce {

tree_impl::tree_impl(be::be &be, maybe_nodeid rootID, mempool &mempool, const tree_functions &fns)
    : m_be(be), m_rootID(rootID), m_mempool(mempool), m_fns(fns)
{
}

//----------------------------------------------------------------------
//  Loading
//

node_ptr &tree_impl::root()
{
    if (!m_root)
    {
        if (m_rootID)
            m_root = load(*m_rootID);
        else
            // No existing tree, create a new leaf node
            m_root = boost::make_shared<LeafNode>(m_fns);
    }

    return m_root;
}

const node_ptr &tree_impl::child(node_branch &branch)
{
    if (!branch.child) branch.child = load(branch.nodeID);
    return branch.child;
}

const node_ptr &tree_impl::overflowNode(overflow_t &overflow)
{
    assert(!overflow.empty());
    if (!overflow.node) overflow.node = load(overflow.nodeID);
    return overflow.node;
}

node_ptr tree_impl::load(nodeid_t id)
{
    m_loadedIDs.push_back(id);
    return deserialize(m_be.get(id));
}

node_ptr tree_impl::deserialize(const mempage &mem)
{
    m_mempool.retain(mem);
    return ParseNode(mem, m_fns);
}

//----------------------------------------------------------------------
//  Editing
//

void tree_impl::apply(const node_ptr &node, const pending_edit &edit, Depth depth)
{
NODE_CASE_LEAF
    switch (edit.edit)
    {
        case INSERT:
            leafInsert(leaf, edit.key, edit.value, false, NULL);
            break;
        case UPSERT:
            leafInsert(leaf, edit.key, edit.value, true, NULL);
            break;
        case REMOVE_KEY:
            leafRemove(leaf, edit.key, NULL, NULL);
            break;
        case REMOVE_KV:
            leafRemove(leaf, edit.key, &edit.value, NULL);
            break;
    }

NODE_CASE_OVERFLOW
    // Shouldn't ever be called, editing the overflow node is taken
    // care of by the LAEF case.
    assert(false);

NODE_CASE_INT
    if (depth == DEEP)
    {
        keycount_t i = FindInternalKey(internal, edit.key, m_fns);
        apply(internal->branches[i].child, edit, depth);
    }
    else
    {
        // Add in sorted location, but at the end of the same keys
        editlist_t::iterator it = std::upper_bound(internal->editQueue.begin(),
                                                internal->editQueue.end(),
                                                edit.key,
                                                EditOrder(m_fns));
        internal->editQueue.insert(it, edit);
    }

NODE_CASE_END
}

void tree_impl::leafInsert(const leafnode_ptr &leaf, const memslice &key, const memslice &value, bool upsert, uint32_t *delta)
{
    pairlist_t::iterator it = leaf->find(key);
    if (upsert && it != leaf->pairs.end())
    {
        // Update
        leaf->update_value(it, value);
    }
    else
    {
        // Insert

        // If we found PAST the final key and there is an overflow block, we need to pull the entire
        // overflow block in and insert it after that.
        if (it == leaf->pairs.end() && !leaf->overflow.empty())
        {
            memslice o_key = leaf->pairs.rbegin()->first;
            while (!leaf->overflow.empty())
            {
                memslice o_value = overflowPull(leaf->overflow);
                leaf->insert(kv_pair(o_key, o_value));
            }
            leaf->insert(kv_pair(key, value));
            if (delta) (*delta)++;
        }
        // If we found the final key, insert into the overflow block
        else if (it == (--leaf->pairs.end()) && !leaf->overflow.empty())
        {
            // There is an overflow block already. Insert in there.
            overflowInsert(leaf->overflow, value, delta);
        }
        else
        {
            leaf->insert(kv_pair(key, value));
            if (delta) (*delta)++;
        }
    }
}

void tree_impl::leafRemove(const leafnode_ptr &leaf, const memslice &key, const memslice *value, uint32_t *delta)
{
    if (leaf->pairs.empty()) return;

    pairlist_t::iterator begin, end;
    leaf->findRange(key, &begin, &end);

    pairlist_t::iterator eraseLocation = leaf->pairs.end();

    // Regular old remove from this block
    for (pairlist_t::iterator it = begin; it != end; ++it)
    {
        if (!value || m_fns.valueCompare(it->second, *value) == 0)
        {
            eraseLocation = it;
            break;
        }
    }

    if (eraseLocation != leaf->pairs.end())
    {
        // Did erase in this block
        eraseLocation = leaf->erase(eraseLocation);

        // If we removed the final position, pull back from the overflow block.
        if (eraseLocation == leaf->pairs.end() && !leaf->overflow.empty())
        {
            memslice ret = overflowPull(leaf->overflow);
            leaf->insert(kv_pair(key, ret));
        }

        if (delta) (*delta)--;
    }
    // If we did not erase here, but the key matches the last key, search in the overflow block
    else if (!leaf->overflow.empty() && key == leaf->pairs.rbegin()->first)
    {
        // Did not erase from this leaf but key matches overflow key, recurse
        return overflowRemove(leaf->overflow, value, delta);
    }
}

void tree_impl::overflowInsert(overflow_t &overflow_rec, const memslice &value, uint32_t *delta)
{
    overflownode_ptr overflow = boost::static_pointer_cast<OverflowNode>(overflowNode(overflow_rec));
    overflow->append(value);
    if (delta) (*delta)++;
    overflow_rec.count = overflow->itemCount();
}

void tree_impl::overflowRemove(overflow_t &overflow_rec, const memslice *value, uint32_t *delta)
{
    overflownode_ptr overflow = boost::static_pointer_cast<OverflowNode>(overflowNode(overflow_rec));

    // Try to remove from this block
    bool erased = false;
    for (valuelist_t::iterator it = overflow->values.begin(); it != overflow->values.end(); ++it)
    {
        if (!value || m_fns.valueCompare(*it, *value) == 0)
        {
            overflow->values.erase(it);
            erased = true;
        }
    }

    // Try to remove from the next block
    if (!erased && !overflow->next.empty())
    {
        overflowRemove(overflow->next, value, delta);
    }

    // If this block is now empty, pull a value from the next one
    if (!overflow->itemCount() && !overflow->next.empty())
    {
        memslice value = overflowPull(overflow->next);
        overflow->append(value);
    }

    overflow_rec.count = overflow->values.size() + overflow->next.count;

    if (erased && delta) (*delta)--;
}

memslice tree_impl::overflowPull(overflow_t &overflow_rec)
{
    overflownode_ptr overflow = boost::static_pointer_cast<OverflowNode>(overflowNode(overflow_rec));

    if (overflow->next.empty())
    {
        memslice ret = overflow->values.back();
        overflow->erase(overflow->valueCount() - 1);
        overflow_rec.count = overflow->values.size();
        return ret;
    }

    memslice ret = overflowPull(overflow->next);
    overflow_rec.count = overflow->values.size() + overflow->next.count;
    return ret;
}

void tree_impl::applyEditsToBranch(const internalnode_ptr &internal, const keycount_t &i)
{
    editlist_t::iterator editBegin = internal->editQueue.begin();
    editlist_t::iterator editEnd = internal->editQueue.end();

    if (i > 0) editBegin = std::lower_bound(internal->editQueue.begin(), internal->editQueue.end(), internal->branches[i].minKey, EditOrder(m_fns));
    if (i < internal->branches.size() - 1) editEnd = std::lower_bound(internal->editQueue.begin(), internal->editQueue.end(), internal->branches[i+1].minKey, EditOrder(m_fns));

    if (editBegin == editEnd) return; // Nothing to apply

    assert(internal->branches[i].child);

    if (internal->branches[i].child->nodeType() == TYPE_LEAF)
        boost::static_pointer_cast<LeafNode>(internal->branches[i].child)->applyAll(editBegin, editEnd);
    else
    {
        for (editlist_t::const_iterator it = editBegin; it != editEnd; ++it)
        {
            apply(internal->branches[i].child, *it, SHALLOW);
        }
    }
}

}
