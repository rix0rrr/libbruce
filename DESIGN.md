Bruce KV based B-tree design
============================

- Stores keys and values.
- Stores counts in intermediate nodes so we can quickly do offset (paging)
  queries.
- Designed s.t. it's reasy to run on DynamoDB, without needing a transactioning
  system. We do structure-sharing updates instead of in-place updates s.t.
  aborts at any point don't need any cleanup beyond recycling newly created
  nodes.

Node types
----------

- Intermediate nodes, contains list of (key, pointer, count).
- Leaf nodes, contains list of (key, value).

Storage
-------

- Pluggable block engines (memory, file, disk).
- Blocks are immutable, but can be recycled.
- Serialization using FlatBuffers.
- Blocks are compressed using libz.
- Cached
- Configurable block sizes (block engines are supposed to throw an exception if
  they can't accept blocks of a particular size).
- Reused blocks are added to a recycler list.

bruce<<cache<file>, 1024>

Public interface
----------------

- bruce (engine plus a reference to a key engine)
    - .new() -> node
    - .get(id) -> node
    - .cleanup(nodelist)

- node (a root path of ids and a reference to the bruce instance)
    - .all() -> iterator
    - .find(key) -> iterator
    - .find(minkey, maxkey_exclusive) -> iterator
    - .insert(key, value) -> mutation
    - .remove(key) -> mutation
    - .parent() -> optional<node>

- iterator (reference to a node and a position in a node, and an end key)
    - .skip(n)
    - operator bool() -> bool
    - .current() -> pair
    - operator++() 

- mutation(success, newroot, newnodes, recyclednodes)
    - .commit() -> nodelist(recyclednodes)
    - .rollback() -> nodelist(newnodes)

Overflow Block Rules
--------------------

The same key can occur multiple times in a single leaf node, but never in more
than one leaf node. To store keys that won't fit anymore in a single leaf node,
leaf nodes can have a linked list of "overflow" blocks trailing them.

INVARIANT
- All values in the overflow node have the same key, which is the same key as
  the last key in the leaf node.

INSERTING
- When splitting, put as much as can go into the overflow block as possible,
  multiple blocks if necessary.
- When inserting at the end and there’s and overflow block, insert into the
  overflow block
- When splitting a leaf, any existing overflow block should go to the right.

REMOVING
- When removing an arbitrary key, prefer removing from the overflow chain if
  present.
- When removing a key/value pair, we may need to shift a key from the overflow
  block to the end of the leaf node (which may lead to a split if that k/v pair
  causes the leaf to bloat).
