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
