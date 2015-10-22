#include "nodes.h"

#include <algorithm>

namespace libbruce {

memslice g_emptyMemory;

int pending_edit::delta() const
{
    return edit == UPSERT ? 0 :
            edit == INSERT ? 1 :
            -1;
}

//----------------------------------------------------------------------

Node::Node(node_type_t nodeType)
    : m_nodeType(nodeType)
{
}

Node::~Node()
{
}

//----------------------------------------------------------------------

std::ostream &operator <<(std::ostream &os, const libbruce::Node &x)
{
    x.print(os);
    return os;
}

}
