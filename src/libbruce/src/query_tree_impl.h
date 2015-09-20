#ifndef BRUCE_QUERY_TREE_H
#define BRUCE_QUERY_TREE_H

#include <map>
#include <vector>
#include <boost/enable_shared_from_this.hpp>

#include <libbruce/types.h>
#include <libbruce/be/be.h>
#include <libbruce/query_tree.h>

#include "tree_impl.h"
#include "nodes.h"
#include "query_iterator_impl.h"

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

struct query_tree_impl : public tree_impl, public boost::enable_shared_from_this<query_tree_impl>
{
    query_tree_impl(be::be &be, nodeid_t rootID, const tree_functions &fns);

    void queue_insert(const memory &key, const memory &value);
    void queue_remove(const memory &key, bool guaranteed);
    void queue_remove(const memory &key, const memory &value, bool guaranteed);

    bool get(const memory &key, memory *value);
    query_iterator_impl_ptr find(const memory &key);

    void applyPendingChanges(const node_ptr &node, const memory &minKey, const memory &maxKey);
private:
    typedef std::vector<pending_edit> editlist_t;
    typedef std::map<memory, editlist_t, callback_memcmp> editmap_t;

    editmap_t m_edits;

    bool findRec(const node_ptr &node, const memory &key, const memory &minKey, const memory &maxKey, std::vector<knuckle> &rootPath, query_iterator_impl_ptr *iter_ptr);
    void applyPendingChange(const leafnode_ptr &leaf, const pending_edit &edit);
};

}

#endif
