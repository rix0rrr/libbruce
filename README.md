# libbruce

`libbruce` is a C++ library implementing an ordered key-value store that is
designed to be used with remote unordered key-value stores (like Amazon's S3 and
DynamoDB). The data is stored in B-tree form, where every page is a separate
object in the remote store.

The tree also retains order statistics, which means it is quick searching to a
specific offset in the tree. This makes `libbruce` very suitable for datasets
that need pagination.

## Contents of this repository

- libbruce, the linkable C++ library.
- awsbruce, storage backends for AWS' storage services.
- s3kv, a command-line tool for accessing a key-value store in S3.
- bstat, a tool for calculating storage efficiency statistics of a bruce tree.

## User Guide

Block engines.

    edit_tree 

    query_tree

### Guaranteed modifications

To ensure fast seek behavior, bruce keeps a list of counts for every branch of
internal tree nodes.

These counts need to be updated when changes are pushed down into those internal
nodes, and we want to do this quickly and cheaply. There's a problem though: we
can only know whether a particular change (such as a remove or upsert) would
affect the item count in the tree when the change arrives at the leaf, because
that's where the actual values are stored. Only then can we know whether the
remove would actually remove a value, or the upsert would update an existing
value instead of adding one.

To avoid having to go to a leaf node for every change, a consumer can promise
that the change is 'guaranteed'. The client promises that the key already exists
in the tree when making the change, so bruce can do the rank calculations
optimistically and won't need to go to the leaves to apply the change.

> Important: if you make a false guarantee, this will affect the `seek()` and
> `rank()` operations of the tree, NOT `find()` operations. If you do not care
> about rank operations, you can always guarantee all changes for maximum
> performance.

### Implementing new stored types

TODO

## Change queue size

To keep updates to the tree quick, edits are queued up along the internal nodes
in the tree, up to a certain amount of bytes. When the queue size is exceeded,
the pending changes are pushed down until they finally reach the leaf nodes.
Pushing changes down is also done on querying, so you are trading write vs. read
performance using this setting.

## Implementation Details

Duplicates pose problems to a B-tree. Just storing the duplicates in the leaf
nodes of the tree poses an algorithmic problem when a key can be found on either
side of an internal node's branch. `libbruce` deals with this by introducing a
new type of node, an 'overflow' node. This node comes into a play when there are
more elements with the same key as the last key in a leaf node; the overflow
nodes form a linked list after the leaf page, containing values for the last key
in the leaf page.

Pages are immutable. This makes the data structure safe for concurrent (attempts
at) modification. When mutating, only the pages on the root path to changed
pages are modified and rewritten, while the rest of the pages are
structure-shared with the previous version of the tree. Only after the ID of the
new root page has been commited somewhere (or failed to have been committed
somewhere, preferably using a compare-and-swap operation), the operation can be
committed or rolled back, which means deleting either the obsoleted or added
pages.

Bruce trees do not have 'next' pointers like B+-trees. Because pages are
immutable, we'd have to rewrite the complete left half of the tree from a
changed page to properly update the 'next' links (since changing the link would
produce a new page and so that page's previous page would need to change, etc).
Instead, efficient forward iteration is achieved by parallel fetching of entire
tree levels at once.

Data in the serialized pages is stored in column order (i.e., first all keys are
stored, then all values are stored) for maximum compression potential. Depending
on the data, a compression ratio of 0.1 can be expected. Block sizes are
specific pre-compression, so adjust block sizes accordingly.

## Code layout

Explain double-indirect classes.

## Build Instructions

Needs the aws-cpp-sdk. Also, it needs to be patched to export the
`aws-cpp-sdk-core` and `aws-cpp-sdk-s3` targets.

    cmake .. -DCMAKE_BUILD_TYPE=DEBUG -DSTATIC_LINKING=1
