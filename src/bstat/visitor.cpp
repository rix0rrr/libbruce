#include "visitor.h"

// Cheekily access the private API of the library
#include "../libbruce/src/serializing.h"

using namespace libbruce;

BruceVisitor::~BruceVisitor()
{
}

void doWalk(const nodeid_t &id, BruceVisitor &visitor, be::be &blockEngine, const tree_functions &fns, int depth)
{
    mempage mem = blockEngine.get(id);
    node_ptr node = ParseNode(mem, fns);
    switch (node->nodeType())
    {
        case TYPE_LEAF:
            {
                leafnode_ptr leaf = boost::static_pointer_cast<LeafNode>(node);
                visitor.visitLeaf(leaf, depth);

                if (!leaf->overflow.empty())
                    doWalk(leaf->overflow.nodeID, visitor, blockEngine, fns, depth+1);

                break;
            }

        case TYPE_INTERNAL:
            {
                internalnode_ptr internal = boost::static_pointer_cast<InternalNode>(node);
                visitor.visitInternal(internal, depth);

                for (branchlist_t::iterator it = internal->branches.begin(); it != internal->branches.end(); ++it)
                {
                    doWalk(it->nodeID, visitor, blockEngine, fns, depth+1);
                }

                break;
            }

        case TYPE_OVERFLOW:
            {
                overflownode_ptr overflow = boost::static_pointer_cast<OverflowNode>(node);
                visitor.visitOverflow(overflow, depth);

                if (!overflow->next.empty())
                    doWalk(overflow->next.nodeID, visitor, blockEngine, fns, depth+1);

                break;
            }

        default:
            throw std::runtime_error("Unrecognized node type");
    }
}

void walk(const nodeid_t &id, BruceVisitor &visitor, be::be &blockEngine, const tree_functions &fns)
{
    doWalk(id, visitor, blockEngine, fns, 1);
}
