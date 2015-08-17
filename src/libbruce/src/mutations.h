/**
 * A population of possible node mutations
 *
 * See operations.h for a rationale.
 */
#ifndef BRUCE_MUTATIONS_H
#define BRUCE_MUTATIONS_H

#include <libbruce/memory.h>
#include <vector>

#include "serializing.h"

namespace bruce { namespace mutations {

/**
 * Base class of page creation actions
 */
struct action
{
    virtual ~action();
};

/**
 * Copy part of a leaf node
 */
struct copy_leaf : public action
{
    copy_leaf(const leafnode_ptr &node, uint32_t start, uint32_t length)
        : m_node(node), m_start(start), m_length(length) { }

private:
    leafnode_ptr m_node;
    uint32_t m_start;
    uint32_t m_length;
};

/**
 * Copy part of a leaf node and insert a pair
 */
struct copy_insert_leaf : public copy_leaf
{
    copy_insert_leaf(const leafnode_ptr &node, uint32_t start, uint32_t length, const range &key, const range &value, uint32_t insertIndex)
        : copy_leaf(node, start, length), m_key(key), m_value(value), m_insertIndex(insertIndex) { }
private:
    leafnode_ptr m_node;
    range m_key;
    range m_value;
    uint32_t m_insertIndex;
};

/**
 * Copy part of an internal node
 */
struct copy_internal : public action
{
    copy_leaf(const internalnode_ptr &node, uint32_t start, uint32_t length)
        : m_node(node), m_start(start), m_length(length) { }
};




typedef boost::shared_ptr<action> action_ptr;

/**
 * Pending mutation class
 */
struct pending_mutations
{
private:
    std::vector<action_ptr> m_actions;

    /**
     * List of fresh IDs to grab
     *
     * While building the mutation list, only the size of this array
     * matters. After fetching the IDs, they can be accessed by
     * index.
     */
    std::vector<nodeident_t> m_freshIDs;

    /**
        * IDs of pages that have been made into garbage by this operation
        */
    std::vector<nodeident_t> m_garbageIDs;
};


}}

#endif
