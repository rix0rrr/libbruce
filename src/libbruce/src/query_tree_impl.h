#ifndef BRUCE_QUERY_TREE_H
#define BRUCE_QUERY_TREE_H

#include <map>
#include <vector>

#include <libbruce/types.h>
#include <libbruce/be/be.h>

#include "tree_impl.h"
#include "nodes.h"

namespace bruce {

enum edit_t {
    INSERT, REMOVE_KEY, REMOVE_KV
};

struct pending_edit
{
    pending_edit(edit_t edit, const memory &key, const memory &value, bool guaranteed)
        : edit(edit), key(key), value(value), guaranteed(guaranteed) { }

    edit_t edit;
    memory key;
    memory value;
    bool guaranteed;
};

struct callback_memcmp
{
    callback_memcmp(const tree_functions &fns) : fns(fns) { }

    bool operator()(const memory &a, const memory &b) const
    {
        if (a.empty()) return true;
        if (b.empty()) return false;

        return fns.keyCompare(a, b) < 0;
    }

    tree_functions fns;
};

struct query_tree_impl : private tree_impl
{
    query_tree_impl(be::be &be, nodeid_t rootID, const tree_functions &fns);

    void queue_insert(const memory &key, const memory &value);
    void queue_remove(const memory &key, bool guaranteed);
    void queue_remove(const memory &key, const memory &value, bool guaranteed);

    bool get(const memory &key, memory *value);
private:
    typedef std::map<memory, std::vector<pending_edit>, callback_memcmp> editmap_t;

    editmap_t m_edits;

    bool getRec(const node_ptr &node, const memory &key, memory *value);
};

}

#endif
