#include "leaf_node.h"
#include <boost/foreach.hpp>

namespace libbruce {

LeafNode::LeafNode(const tree_functions &fns)
    : Node(TYPE_LEAF), m_before(fns), m_elementsSize(0)
{
}

LeafNode::LeafNode(pairlist_t::const_iterator begin, pairlist_t::const_iterator end, const tree_functions &fns)
    : Node(TYPE_LEAF), m_before(fns), pairs(begin, end), m_elementsSize(0)
{
    calcSize();
}

LeafNode::LeafNode(std::vector<kv_pair> *v, const tree_functions &fns)
    : Node(TYPE_LEAF), m_before(fns), m_elementsSize(0)
{
    // Do a swap to avoid memory copies
    pairs.swap(*v);
    calcSize();
}

void LeafNode::calcSize()
{
    for (pairlist_t::const_iterator it = pairs.begin(); it != pairs.end(); ++it)
    {
        m_elementsSize += it->first.size() + it->second.size();
    }
}

const memslice &LeafNode::minKey() const
{
    if (pairs.size()) return pairs.begin()->first;
    return g_emptyMemory;
}

itemcount_t LeafNode::itemCount() const
{
    return overflow.count + pairCount();
}

void LeafNode::setOverflow(const node_ptr &node)
{
    overflow.node = node;
    overflow.count = node->itemCount();
    if (!overflow.count)
    {
        overflow.node = node_ptr();
        overflow.count = 0;
    }
}

pairlist_t::const_iterator LeafNode::get_at(int n) const
{
    pairlist_t::const_iterator it = pairs.begin();
    for (int i = 0; i < n && it != pairs.end(); ++it, ++i);
    return it;
}

pairlist_t::iterator LeafNode::get_at(int n)
{
    pairlist_t::iterator it = pairs.begin();
    for (int i = 0; i < n && it != pairs.end(); ++it, ++i);
    return it;
}

void LeafNode::findRange(const memslice &key, pairlist_t::iterator *begin, pairlist_t::iterator *end)
{
    *begin = std::lower_bound(pairs.begin(), pairs.end(), key, m_before);
    *end = std::upper_bound(pairs.begin(), pairs.end(), key, m_before);
}

/**
 * Apply all edits onto the current leaf by doing a merge of the current pairs and the edits
 *
 * This is necessary for getting good performance (so we don't have to shift too many array items
 * on every insert).
 */
void LeafNode::applyAll(const editlist_t::iterator &editBegin, const editlist_t::iterator &editEnd)
{
    pairlist_t::iterator copy = pairs.begin();
    editlist_t::iterator edit = editBegin;

    // Reserve enough memory for worst-case scenario so we never reallocate
    pairlist_t updated;
    updated.reserve(pairs.size() + editEnd - editBegin);
    size_t newSize = 0;

    while (copy != pairs.end() || edit != editEnd)
    {
        // Deletes come from the end of the 'updated' array, but they can
        // only apply when the remove key is < the copy key (the next insert key).
        if (edit != editEnd && edit->edit != INSERT && (copy == pairs.end() || m_before(edit->key, *copy)))
        {
            bool didUpsert = false;

            // Normally I'd write a reverse_iterator loop, but for some reason they're giving me
            // memory trouble on clang (yet not on gcc, and valgrind is also clean).
            for (int i = updated.size(); i > 0 && updated[i - 1].first == edit->key; i--)
            {
                pairlist_t::iterator it = updated.begin() + i - 1;

                if (edit->edit == UPSERT)
                {
                    newSize -= it->second.size();
                    newSize += edit->value.size();
                    it->second = edit->value;
                    didUpsert = true;
                    break;
                }

                // Deletes
                if (edit->edit == REMOVE_KEY || (edit->edit == REMOVE_KV && edit->value == it->second))
                {
                    newSize -= it->first.size() + it->second.size();
                    updated.erase(it);
                    break;
                }
            }

            if (edit->edit == UPSERT && !didUpsert)
            {
                // Turn upsert into an insert
                updated.push_back(kv_pair(edit->key, edit->value));
                newSize += edit->key.size() + edit->value.size();
            }

            ++edit;
            continue;
        }

        // Inserts are taken from the front of the edit queue or the copy queue
        if (edit != editEnd && edit->edit == INSERT)
        {
            int keyComp = -1; // If there are no more items to compare to, be sure to insert
            if (copy != pairs.end())
                keyComp = m_before.fns.keyCompare(edit->key, copy->first);

            if (keyComp <= 0)
            {
                // Insert
                updated.push_back(kv_pair(edit->key, edit->value));
                newSize += edit->key.size() + edit->value.size();
                ++edit;
                continue;
            }
        }

        // No update applies -- regular copy
        if (copy != pairs.end())
        {
            updated.push_back(*copy);
            newSize += copy->first.size() + copy->second.size();
            ++copy;
        }
    }

    pairs.swap(updated);
    m_elementsSize = newSize;
}

void LeafNode::print(std::ostream &os) const
{
    os << "LEAF(" << pairCount() << ")" << std::endl;
    BOOST_FOREACH(const libbruce::kv_pair &p, pairs)
        os << "  " << p.first << " -> " << p.second << std::endl;
    if (!overflow.empty())
        os << "  Overflow " << overflow.count << " @ " << overflow.nodeID << std::endl;
}

}
