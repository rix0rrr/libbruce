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

std::ostream &operator <<(std::ostream &os, const libbruce::pending_edit &e)
{
    switch (e.edit)
    {
        case INSERT: os << "INSERT"; break;
        case UPSERT: os << "UPSERT"; break;
        case REMOVE_KEY: os << "REMOVE"; break;
        case REMOVE_KV: os << "REMOVE"; break;
    }

    os << " " << e.key;
    if (e.value.size()) os << ", " << e.value;
    os << " (" << (e.guaranteed ? "guaranteed" : "speculative") << ")";

    return os;
}

}
